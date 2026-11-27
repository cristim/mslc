#include "sema/emitter.h"

namespace mslc {

bool isArrayKind(ResourceKind kind) {
	return kind == ResourceKind::Texture2DArray || kind == ResourceKind::Texture1DArray
		|| kind == ResourceKind::TextureCubeArray;
}
bool isCubeKind(ResourceKind kind) {
	return kind == ResourceKind::TextureCube || kind == ResourceKind::TextureCubeArray;
}
bool isOneDimensional(ResourceKind kind) {
	return kind == ResourceKind::Texture1D || kind == ResourceKind::Texture1DArray;
}

// write(value, coordinate [, layer]) in statement position, on an access::write or
// access::read_write texture. The value is a vector of exactly the texture's own
// element type, or any number, converted to that type and put in every channel
// (measured with Apple's compiler: half4 into a float texture and uint4 into an int
// one are refused, a float scalar into a half or int texture is not). The coordinate is a uint or ushort vector of the
// texture's dimension; an array or a cube takes a uint2 and then the layer or
// face. An explicit lod is not lowered.
void Emitter::emitTextureWrite(const Expression& call, const Binding& texture) {
	const TextureAccess access = texture.pointeeMsl.textureAccess;
	if (access != TextureAccess::Write && access != TextureAccess::ReadWrite) {
		throw CompileError("the texture method \"write\" is not lowered yet on textures that are not "
			"access::write or access::read_write");
	}
	const ResourceKind kind = texture.pointeeMsl.resource;
	const size_t layers = layerArguments(kind);
	const uint32_t width = storageCoordinateWidth(kind);
	const std::string suffix = width == 1 ? std::string() : std::to_string(width);
	const ScalarKind element = texture.pointeeMsl.scalar;
	const ScalarKind sampled = textureSampledKind(element);
	const std::string takes = std::string("texture write takes exactly ") + (element == ScalarKind::Int ? "an " : "a ") + scalarKindName(element)
		+ "4 or a number and a uint" + suffix + " or ushort" + suffix + " coordinate"
		+ (layers == 2 ? " and a face and an array index" : layers == 1 ? (kind == ResourceKind::TextureCube ? " and a face" : " and a layer") : "");
	if (call.arguments.size() == 3 + layers && kind == ResourceKind::TextureBuffer) {
		throw CompileError("write on a texture_buffer takes a value and one coordinate: a texture_buffer has no mip levels");
	}
	if (call.arguments.size() == 3 + layers) {
		throw CompileError("mip arguments are unsupported on access::" + std::string(textureAccessName(access))
			+ " textures: Apple accepts the level, but a Vulkan storage image view is one level, so the host binds it; "
			"write takes only a value, a coordinate and a layer where the texture has one");
	}
	if (call.arguments.size() != 2 + layers) {
		throw CompileError(takes);
	}
	const Id value = emitNonComponent(*call.arguments[0], "texture write");
	Id coordinate = emitExpression(*call.arguments[1]);
	const Id coordinateType = _builder.typeOf(coordinate);
	const Id valueType = _builder.typeOf(value);
	const bool unsignedVector = _types.vectorWidth(coordinateType) == width && !_types.isFloat(coordinateType)
		&& !_types.isSignedInt(coordinateType) && (_types.bitWidth(coordinateType) == 32 || _types.bitWidth(coordinateType) == 16);
	const bool numeric = _types.vectorWidth(valueType) == 1;
	const bool ownVector = valueType == _types.vector(element, 4);
	if (!unsignedVector || !(numeric || ownVector)) {
		throw CompileError(takes);
	}
	coordinate = convert(coordinate, coordinateType, _types.vector(ScalarKind::UInt, width));
	coordinate = appendLayers(coordinate, kind, call.arguments, 2, "write");
	// The image's sampled type is the 32-bit type of the element, and a number is
	// narrowed to the element type before it is widened to it and put in every
	// channel, so a half or short texture stores the value rounded to its own type.
	const Id texelType = _types.vector(sampled, 4);
	Id texel;
	if (ownVector) {
		texel = convert(value, valueType, texelType);
	} else {
		texel = broadcast(convert(value, valueType, _types.scalar(element)), texelType);
	}
	const Id image = loadFrom(texture.id, texture.pointeeType);
	_builder.emit(spirv::OpImageWrite, { image, coordinate, texel });
}

// texture.fence(): orders prior image operations in Device scope. Apple defines
// fence() on colour textures and texture_buffer across all accesses, and omits it on
// depth and multisampled textures.
void Emitter::emitTextureFence(const Expression& call, const Binding& texture) {
	if (texture.pointeeMsl.isDepth || texture.pointeeMsl.isMultisampled) {
		throw CompileError("the " + typeName(texture.pointeeMsl) + " method \"fence\" is not lowered: "
			"depth and multisampled textures have no fence");
	}
	if (!call.arguments.empty()) {
		throw CompileError("texture fence takes no arguments");
	}
	// Device (1), AcquireRelease (0x8) | ImageMemory (0x800) = 2056.
	_builder.emit(spirv::OpMemoryBarrier, { constantU32(1), constantU32(8 | 2048) });
}

Id Emitter::emitTextureCall(const Expression& call) {
	const Binding& texture = textureReceiver(call);
	const std::string& method = call.left->memberName;
	const ResourceKind kind = texture.pointeeMsl.resource;
	const TextureAccess access = texture.pointeeMsl.textureAccess;
	if (method == "write") {
		throw CompileError("texture write returns void and has no value");
	}
	if (method == "fence") {
		if (texture.pointeeMsl.isDepth || texture.pointeeMsl.isMultisampled) {
			throw CompileError("the " + typeName(texture.pointeeMsl) + " method \"fence\" is not lowered: "
				"depth and multisampled textures have no fence");
		}
		throw CompileError("texture fence returns void and has no value");
	}
	if (texture.pointeeMsl.isMultisampled) {
		return emitMultisampledCall(call, texture);
	}
	if (kind == ResourceKind::TextureBuffer && method != "read" && method != "get_width") {
		throw CompileError("a texture_buffer has read, write and get_width only; \"" + method
			+ "\" is not a texture_buffer method");
	}
	const bool sized = method == "get_width" || method == "get_height"
		|| (method == "get_depth" && kind == ResourceKind::Texture3D)
		|| (method == "get_array_size" && isArrayKind(kind));
	if (access == TextureAccess::Write && !sized) {
		throw CompileError("write-only textures lower only the size queries and statement "
			"write(value, coordinate); \"" + method + "\" is not lowered yet");
	}
	if (access != TextureAccess::Sample && access != TextureAccess::Write && method != "read" && !sized
		&& !(access == TextureAccess::Read && method == "get_num_mip_levels")) {
		if (method == "get_num_mip_levels") {
			throw CompileError(std::string("access::") + textureAccessName(access) + " textures lower only read(), "
				"the size queries and statement write(); \"get_num_mip_levels\" is refused because a Vulkan "
				"storage image view is a single level, so OpImageQueryLevels would answer 1 where Metal's texture "
				"answers its full level count");
		}
		throw CompileError(std::string("access::") + textureAccessName(access) + " textures lower only read(), "
			+ (access == TextureAccess::Read ? "get_num_mip_levels() and " : "statement write() and ")
			+ "the size queries; \"" + method + "\" is not lowered yet");
	}
	if (method == "sample" && access == TextureAccess::Sample) {
		return emitTextureSample(call, texture);
	}
	if (method == "gather" && access == TextureAccess::Sample) {
		return emitTextureGather(call, texture);
	}
	if (method == "sample_compare" && access == TextureAccess::Sample && texture.pointeeMsl.isDepth) {
		return emitTextureSample(call, texture, true);
	}
	if (method == "gather_compare" && access == TextureAccess::Sample && texture.pointeeMsl.isDepth) {
		return emitTextureGather(call, texture, true);
	}
	if (method == "get_num_mip_levels" && !texture.pointeeMsl.isStorageTexture()) {
		if (!call.arguments.empty()) {
			throw CompileError("get_num_mip_levels takes no arguments");
		}
		if (!_imageQueryDeclared) {
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageQuery) });
			_imageQueryDeclared = true;
		}
		return _builder.emitTyped(spirv::OpImageQueryLevels, _uintType, { loadFrom(texture.id, texture.pointeeType) });
	}
	if (method == "get_height" && isOneDimensional(kind)) {
		throw CompileError(std::string("a ") + resourceKindName(kind) + " has no get_height; it is one texel high");
	}
	if (method == "get_width" || method == "get_height") {
		return emitTextureSize(call, texture,
			method == "get_width" ? SizeComponent::Width : SizeComponent::Height);
	}
	if (method == "read" && (access != TextureAccess::Sample || !isCubeKind(kind))) {
		return emitTextureRead(call, texture);
	}
	if (method == "get_depth" && kind == ResourceKind::Texture3D) {
		return emitTextureSize(call, texture, SizeComponent::Depth);
	}
	if (method == "get_array_size" && isArrayKind(kind)) {
		if (!call.arguments.empty()) {
			throw CompileError("get_array_size takes no arguments; the layer count has no mip level");
		}
		return emitTextureSize(call, texture, SizeComponent::ArraySize);
	}

	if (isCubeKind(kind)) {
		throw CompileError(std::string("the ") + resourceKindName(kind) + " method \"" + method
			+ "\" is not lowered yet; mslc lowers sample, gather, get_width and get_height on a cube "
			"and get_array_size on a cube array"
			+ (method == "read" ? "; read is refused because Vulkan's OpImageFetch does not take a Cube image" : ""));
	}
	throw CompileError("the texture method \"" + method + "\" is not lowered yet; mslc lowers "
		"sample, read, get_width, get_height, get_depth on a texture3d and get_array_size on an "
		"array texture");
}

// The components of the coordinate argument of sample and read on an image of
// this kind. A texture2d_array takes its layer as a separate argument, so its
// coordinate is two wide and its size three; a cube's coordinate is a
// direction, three wide, and its size two.
uint32_t textureCoordinateWidth(ResourceKind kind) {
	switch (kind) {
		case ResourceKind::Texture1D: return 1;
		case ResourceKind::Texture1DArray: return 1;
		case ResourceKind::TextureBuffer: return 1;
		case ResourceKind::TextureCubeArray: return 3;
		case ResourceKind::Texture2D: return 2;
		case ResourceKind::Texture2DArray: return 2;
		case ResourceKind::TextureCube: return 3;
		case ResourceKind::Texture3D: return 3;
		case ResourceKind::None:
		case ResourceKind::Sampler: break;
	}
	throw CompileError(std::string("\"") + resourceKindName(kind) + "\" has no texel coordinate");
}

// A texture1d has one level, and Apple's headers take its lod only as the
// constant zero (read(uint, 0) is accepted, read(uint, 1) and a variable are not).
void requireTexture1DLevelZero(const Expression& lod, const std::string& callName) {
	if (lod.kind != ExpressionKind::IntLiteral || lod.intValue != 0) {
		throw CompileError("the lod of " + callName + " on a texture1d has to be the literal 0: "
			"a texture1d has one level");
	}
}

// A cube storage image keeps the cube view type, but is read and written by texel
// and face: a uint2 and a face in Metal (a cube array adds the array index), a
// three-component coordinate in SPIR-V.
size_t layerArguments(ResourceKind kind) {
	return kind == ResourceKind::TextureCubeArray ? 2
		: kind == ResourceKind::Texture2DArray || kind == ResourceKind::Texture1DArray || kind == ResourceKind::TextureCube ? 1 : 0;
}

uint32_t storageCoordinateWidth(ResourceKind kind) {
	return isCubeKind(kind) ? 2 : textureCoordinateWidth(kind);
}

uint32_t textureSizeWidth(ResourceKind kind) {
	return kind == ResourceKind::Texture1DArray ? 2
		: kind == ResourceKind::Texture2DArray || kind == ResourceKind::TextureCubeArray ? 3
		: kind == ResourceKind::TextureCube ? 2 : textureCoordinateWidth(kind);
}

// A texture2d_array coordinate of `component` with its layer appended as the
// last component, which is where an arrayed image instruction takes it. The
// layer is a uint in Metal, so an int wraps to uint before a sample sees it.
Id Emitter::layerIndex(const Expression& layer, const std::string& callName, const char* what) {
	const Id value = emitNonComponent(layer, callName);
	const Id valueType = _builder.typeOf(value);
	if (_types.vectorWidth(valueType) != 1 || _types.isFloat(valueType) || _types.bitWidth(valueType) < 8) {
		throw CompileError(std::string("the ") + what + " of " + callName + " has to be an integer");
	}

	return convert(value, valueType, _uintType);
}

Id Emitter::appendArrayLayer(Id coordinate, ScalarKind component, const Expression& layer,
	const std::string& callName) {
	const Id index = layerIndex(layer, callName);
	return _builder.emitTyped(spirv::OpCompositeConstruct,
		_types.vector(component, _types.vectorWidth(_builder.typeOf(coordinate)) + 1),
		{ coordinate, convert(index, _uintType, _types.scalar(component)) });
}

// The integer coordinate of read and write with the arguments that follow it, a
// layer or a face, appended. A cube array takes a face and an array index, which
// SPIR-V has as the one layer-face 6 * index + face.
Id Emitter::appendLayers(Id coordinate, ResourceKind kind, const std::vector<ExpressionPtr>& arguments,
	size_t first, const std::string& callName) {
	const size_t layers = layerArguments(kind);
	if (layers == 0) {
		return coordinate;
	}
	if (layers == 1) {
		return appendArrayLayer(coordinate, ScalarKind::UInt, *arguments[first], callName);
	}
	const Id face = layerIndex(*arguments[first], callName);
	const Id index = layerIndex(*arguments[first + 1], callName);
	const Id layerFace = _builder.emitTyped(spirv::OpIAdd, _uintType,
		{ _builder.emitTyped(spirv::OpIMul, _uintType, { index, constantU32(6) }), face });
	return _builder.emitTyped(spirv::OpCompositeConstruct,
		_types.vector(ScalarKind::UInt, _types.vectorWidth(_builder.typeOf(coordinate)) + 1), { coordinate, layerFace });
}


// A texel offset written as an int vector of integer literals, as the OpConstantComposite
// ConstOffset takes, or InvalidId for any other expression. Vulkan guarantees offsets of
// -8 to 7 only, which is Metal's documented range, so a literal beyond it is refused.
Id Emitter::literalOffset(const Expression& expression, uint32_t width) {
	const bool shape = expression.kind == ExpressionKind::Construct && expression.constructType
		&& expression.constructType->scalar == ScalarKind::Int && expression.constructType->vectorWidth == width
		&& (expression.arguments.size() == width || expression.arguments.size() == 1);
	if (!shape) {
		return InvalidId;
	}
	std::vector<uint32_t> parts;
	for (const ExpressionPtr& argument: expression.arguments) {
		int32_t value = 0;
		if (!literalInteger(*argument, value)) {
			return InvalidId;
		}
		if (value < -8 || value > 7) {
			throw CompileError("the texel offset " + std::to_string(value) + " is outside -8 to 7, the range Vulkan "
				"guarantees and Metal documents");
		}
		auto cached = _intConstants.find(value);
		if (cached == _intConstants.end()) {
			cached = _intConstants.emplace(value,
				_builder.emitDeclTyped(spirv::OpConstant, _intType, { static_cast<uint32_t>(value) })).first;
		}
		parts.push_back(cached->second);
	}
	parts.resize(width, parts.front());
	return _builder.emitDeclTyped(spirv::OpConstantComposite, _types.vector(ScalarKind::Int, width), parts);
}

// The sampler a sample or gather names as its first argument. A coord::pixel sampler
// is only lowered on a texture2d (Vulkan allows unnormalized coordinates on
// non-arrayed 1D and 2D views only).
const Binding& Emitter::samplerArgument(const Expression& argument, ResourceKind kind, const std::string& callName) {
	if (argument.kind != ExpressionKind::Identifier) {
		throw CompileError("the first argument of " + callName + " has to name a sampler");
	}
	const Binding* samplerBinding = findResourceBinding(argument.name, argument.fileScope);
	if (!samplerBinding || samplerBinding->pointeeMsl.resource != ResourceKind::Sampler) {
		throw CompileError("\"" + argument.name + "\" is not a sampler, and " + callName + " takes one "
			"as its first argument");
	}
	if (kind != ResourceKind::Texture2D && samplerBinding->unnormalizedSampler) {
		throw CompileError(std::string("a ") + resourceKindName(kind) + " cannot be sampled with a "
			"coord::pixel sampler: mslc lowers unnormalized coordinates on texture2d only (Vulkan "
			"allows them on non-arrayed 1D and 2D image views). A sampler parameter is not checked, since its state is the host's");
	}
	return *samplerBinding;
}

// The float coordinate of a sample or gather, a number being broadcast, with the
// array layer appended where the texture has one.
Id Emitter::samplingCoordinate(const Expression& argument, ResourceKind kind, const Expression* layer,
	const std::string& callName) {
	const uint32_t width = textureCoordinateWidth(kind);
	const Id coordinateVector = _types.vector(ScalarKind::Float, width);
	Id coordinate = emitNonComponent(argument, callName);
	const Id coordinateType = _builder.typeOf(coordinate);
	if (coordinateType != coordinateVector) {
		if (_types.vectorWidth(coordinateType) != 1 || _types.bitWidth(coordinateType) < 8) {
			throw CompileError("the coordinate of " + callName + " has to be a float"
				+ (width == 1 ? std::string() : std::to_string(width)) + " or a number");
		}
		coordinate = width == 1 ? convert(coordinate, coordinateType, coordinateVector)
			: broadcast(coordinate, coordinateVector);
	}
	if (layer) {
		coordinate = appendArrayLayer(coordinate, ScalarKind::Float, *layer, callName);
	}
	return coordinate;
}

// gather(sampler, coordinate [, array index] [, int2 offset] [, component::x..w]): the
// four texels of a 2x2 footprint at one channel, as OpImageGather. The offset is a
// texture2d and texture2d_array argument; a literal one is ConstOffset, any other an
// Offset operand, which needs ImageGatherExtended. A coord::pixel sampler is refused.
Id Emitter::emitTextureGather(const Expression& call, const Binding& texture, bool compare) {
	const auto& arguments = call.arguments;
	const ResourceKind kind = texture.pointeeMsl.resource;
	const bool arrayed = kind == ResourceKind::Texture2DArray || kind == ResourceKind::TextureCubeArray;
	if (kind != ResourceKind::Texture2D && kind != ResourceKind::TextureCube && !arrayed) {
		throw CompileError(std::string("gather is defined on texture2d, texture2d_array, texturecube and texturecube_array; ")
			+ resourceKindName(kind) + " has none");
	}
	const size_t fixed = (arrayed ? 3 : 2) + (compare ? 1 : 0);
	const std::string callName = compare ? "gather_compare" : "gather";
	if (arguments.size() < fixed) {
		throw CompileError(callName + " takes a sampler, a coordinate" + (arrayed ? ", an array index" : "")
			+ (compare ? ", a comparison value" : "") + ", and optionally an int2 offset" + (compare ? "" : " and a component"));
	}
	const Binding& sampler = samplerArgument(*arguments[0], kind, callName);
	if (sampler.unnormalizedSampler) {
		throw CompileError(callName + " through a coord::pixel sampler is not lowered: Vulkan allows no gather on an "
			"unnormalized sampler");
	}
	const Id coordinate = samplingCoordinate(*arguments[1], kind, arrayed ? arguments[2].get() : nullptr, callName);
	const Id reference = compare ? compareValue(*arguments[fixed - 1], callName) : InvalidId;

	Id offset = InvalidId;
	bool constantOffset = false;
	uint32_t component = 0;
	bool componentSeen = false;
	for (size_t i = fixed; i < arguments.size(); ++i) {
		const Expression& argument = *arguments[i];
		const auto givenEnum = enumOf(argument);
		const bool isComponent = givenEnum && *givenEnum == "metal::component";
		if (isComponent && (compare || texture.pointeeMsl.isDepth)) {
			throw CompileError(compare ? "gather_compare takes no component: it compares the depth channel"
				: "gather on a depth texture takes no component: it gathers the depth channel");
		}
		if (isComponent) {
			// Apple takes the component only after the offset, where there is one.
			if (componentSeen || i != (isCubeKind(kind) ? fixed : fixed + 1)) {
				throw CompileError("gather takes one component::x, y, z or w, after the offset where the texture has one");
			}
			if (!canFoldExpression(argument)) {
				throw CompileError("gather requires a constant component::x, y, z or w");
			}
			const FoldedConstant folded = foldExpression(argument);
			if (folded.isComposite || isFloatKind(folded.scalar)
				|| (isSignedInteger(folded.scalar) && static_cast<int64_t>(folded.integer) < 0)
				|| folded.integer > 3) {
				throw CompileError("gather takes one component::x, y, z or w (value 0 to 3)");
			}
			component = static_cast<uint32_t>(folded.integer);
			componentSeen = true;
		} else if (i == fixed && !isCubeKind(kind)) {
			offset = literalOffset(argument, 2);
			if (offset != InvalidId) {
				constantOffset = true;
			} else {
				offset = emitExpression(argument);
				if (_builder.typeOf(offset) != _types.vector(ScalarKind::Int, 2)) {
					throw CompileError("the offset of gather has to be an int2");
				}
			}
		} else {
			throw CompileError(std::string("gather takes ") + (isCubeKind(kind)
				? "a sampler, a coordinate and optionally a component; a cube has no offset"
				: "a sampler, a coordinate, an optional int2 offset and an optional component"));
		}
	}

	const Id image = loadFrom(texture.id, texture.pointeeType);
	const Id loadedSampler = loadFrom(sampler.id, sampler.pointeeType);
	const Id combined = _builder.emitTyped(spirv::OpSampledImage,
		_types.sampledImage(kind, textureSampledKind(texture.pointeeMsl.scalar), texture.pointeeMsl.isDepth, false),
		{ image, loadedSampler });
	std::vector<uint32_t> words { combined, coordinate, compare ? reference : constantU32(component) };
	if (offset != InvalidId && constantOffset) {
		words.push_back(0x8);  // ConstOffset
		words.push_back(offset);
	} else if (offset != InvalidId) {
		if (!_gatherExtendedDeclared) {
			_gatherExtendedDeclared = true;
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageGatherExtended) });
		}
		words.push_back(kImageOperandsOffset);
		words.push_back(offset);
	}
	return texturePixels(texture, _builder.emitTyped(compare ? spirv::OpImageDrefGather : spirv::OpImageGather,
		_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4), words));
}

// sample(sampler, float2 coordinate [, level(lod)]), a float3 coordinate on a
// texturecube or texture3d, or a float2 coordinate and a uint layer on a
// texture2d_array. A fragment function takes
// the level from the derivatives, which is OpImageSampleImplicitLod; a vertex
// or kernel function has none, so it samples level 0 with an explicit lod,
// and so does a sampler with pixel coordinates, which Vulkan does not allow
// an implicit lod on.
// An integer written as a literal, with an optional minus: the only constant a
// texel offset can be here, since SPIR-V takes it as an OpConstant.
bool literalInteger(const Expression& expression, int32_t& value) {
	if (expression.kind == ExpressionKind::IntLiteral && expression.enumName != "metal::component") {
		value = static_cast<int32_t>(expression.intValue);
		return expression.intValue <= INT32_MAX;
	}
	if (expression.kind == ExpressionKind::Unary && expression.unaryOperator == UnaryOperator::Negate
		&& expression.left && literalInteger(*expression.left, value)) {
		value = -value;
		return true;
	}
	return false;
}

} // namespace mslc
