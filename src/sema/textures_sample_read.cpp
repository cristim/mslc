#include "sema/emitter.h"

namespace mslc {

// sample(sampler, coordinate [, array index] [, option] [, min_lod_clamp(m)] [, offset]).
// The option is level(lod), bias(b), or gradient2d, gradient3d or gradientcube
// for the texture's dimension. A fragment function with no option takes the level
// from the derivatives, which is OpImageSampleImplicitLod; a vertex or kernel
// function has none, so it samples level 0 with an explicit lod, and so does a
// sampler with pixel coordinates, which Vulkan does not allow an implicit lod on.
// In a vertex or kernel function bias(b) lowers to explicit Lod b and min_lod_clamp
// without gradients is dropped, matching Apple GPU; an offset has to be a vector of
// integer literals (ConstOffset).
// The comparison value of sample_compare and gather_compare, a float.
Id Emitter::compareValue(const Expression& argument, const std::string& callName) {
	const Id value = emitNonComponent(argument, callName);
	const Id type = _builder.typeOf(value);
	if (_types.vectorWidth(type) != 1 || _types.bitWidth(type) < 8) {
		throw CompileError("the comparison value of " + callName + " has to be a number");
	}
	return convert(value, type, _types.scalar(ScalarKind::Float));
}

Id Emitter::emitTextureSample(const Expression& call, const Binding& texture, bool compare) {
	const auto& arguments = call.arguments;
	const ResourceKind kind = texture.pointeeMsl.resource;
	const bool arrayed = isArrayKind(kind);
	const size_t fixed = (arrayed ? 3 : 2) + (compare ? 1 : 0);
	if (arguments.size() < fixed || arguments.size() > fixed + 3) {
		throw CompileError(std::string("sample takes a sampler, a coordinate")
			+ (arrayed ? ", an array index" : "") + " and optionally an option, min_lod_clamp(m) and an offset; "
			"this call passes " + std::to_string(arguments.size()));
	}

	const std::string callName = compare ? "sample_compare" : "sample";
	const Binding& sampler = samplerArgument(*arguments[0], kind, callName);
	const Id coordinate = samplingCoordinate(*arguments[1], kind, arrayed ? arguments[2].get() : nullptr, callName);
	const Id reference = compare ? compareValue(*arguments[fixed - 1], callName) : InvalidId;

	if (isOneDimensional(kind) && arguments.size() > fixed) {
		throw CompileError(std::string("a ") + resourceKindName(kind) + " samples with a sampler and a coordinate only; "
			"a sample option such as level(lod) is not accepted on it");
	}

	const Id floatType = _types.scalar(ScalarKind::Float);
	const auto scalarArgument = [&](const Expression& option, const std::string& what) {
		if (option.arguments.size() != 1) {
			throw CompileError(what + " takes one argument");
		}
		const Id value = emitNonComponent(*option.arguments[0], what);
		const Id valueType = _builder.typeOf(value);
		if (_types.vectorWidth(valueType) != 1 || _types.bitWidth(valueType) < 8) {
			throw CompileError(what + " takes a scalar number");
		}
		return convert(value, valueType, floatType);
	};
	const uint32_t gradientWidth = kind == ResourceKind::Texture3D || isCubeKind(kind) ? 3 : 2;
	const std::string gradientName = kind == ResourceKind::Texture3D ? "gradient3d"
		: isCubeKind(kind) ? "gradientcube" : "gradient2d";

	Id lod = InvalidId, bias = InvalidId, minLod = InvalidId, gradientX = InvalidId, gradientY = InvalidId, offset = InvalidId;
	int stage = 0;  // the option, then min_lod_clamp, then the offset, in this order
	for (size_t i = fixed; i < arguments.size(); ++i) {
		const Expression& option = *arguments[i];
		const bool isCall = option.kind == ExpressionKind::Call && option.left->kind == ExpressionKind::Identifier;
		const std::string name = isCall ? option.left->name : std::string();
		if (isCall && (name == "level" || name == "bias" || name == "gradient2d" || name == "gradient3d" || name == "gradientcube")) {
			if (stage > 0) {
				throw CompileError("the sample option " + name + "(...) has to come first, before min_lod_clamp and the offset");
			}
			stage = 1;
			if (name == "level") {
				lod = scalarArgument(option, "level");
			} else if (name == "bias") {
				bias = scalarArgument(option, "bias");
			} else {
				if (name != gradientName) {
					throw CompileError(std::string("a ") + resourceKindName(kind) + " takes " + gradientName
						+ "(...), not " + name + "(...)");
				}
				if (option.arguments.size() != 2) {
					throw CompileError(name + " takes two gradients");
				}
				const Id vectorType = _types.vector(ScalarKind::Float, gradientWidth);
				Id* targets[2] = { &gradientX, &gradientY };
				for (size_t g = 0; g < 2; ++g) {
					*targets[g] = emitExpression(*option.arguments[g]);
					if (_builder.typeOf(*targets[g]) != vectorType) {
						throw CompileError(name + " takes two float" + std::to_string(gradientWidth) + " gradients");
					}
				}
			}
		} else if (isCall && name == "min_lod_clamp") {
			if (stage > 1 || minLod != InvalidId) {
				throw CompileError("min_lod_clamp(...) comes after the option and before the offset, once");
			}
			stage = 2;
			minLod = scalarArgument(option, "min_lod_clamp");
		} else {
			if (stage > 2 || offset != InvalidId || isCubeKind(kind)) {
				throw CompileError(isCubeKind(kind) ? "a cube sample takes no offset"
					: "the sample argument after the offset is not lowered");
			}
			stage = 3;
			const uint32_t offsetWidth = kind == ResourceKind::Texture3D ? 3 : 2;
			offset = literalOffset(option, offsetWidth);
			if (offset == InvalidId) {
				throw CompileError("the offset of sample has to be written as an int" + std::to_string(offsetWidth)
					+ " of integer literals: Vulkan takes a constant texel offset");
			}
		}
	}

	const bool derivatives = bias != InvalidId || (lod == InvalidId && gradientX == InvalidId);
	if (sampler.unnormalizedSampler && compare) {
		throw CompileError("sample_compare through a coord::pixel sampler is not lowered: Vulkan allows no depth "
			"comparison on an unnormalized sampler");
	}
	if (sampler.unnormalizedSampler && (bias != InvalidId || minLod != InvalidId || gradientX != InvalidId || offset != InvalidId)) {
		throw CompileError("a coord::pixel sampler takes level(lod) or no option only: Vulkan fixes Lod 0 on it");
	}
	if (minLod != InvalidId && lod != InvalidId) {
		throw CompileError("min_lod_clamp(...) with level(...) is not lowered: Vulkan takes MinLod with an "
			"implicit lod or gradients only");
	}
	const bool library = _entryPoint == &_libraryScope;
	if (derivatives && !sampler.unnormalizedSampler && (bias != InvalidId || minLod != InvalidId)) {
		if (library) {
			throw StageError("bias or min_lod_clamp without gradients in a library helper: their lowering depends on "
				"the stage of the entry point that calls it, which a library does not know");
		}
		// Measured on Apple M1 Pro GPU (multi-mip textures, nearest and linear mip filters):
		// in vertex and kernel functions (and helpers called from them) there are no derivatives,
		// plain sample takes level 0, bias(b) samples level b (clamped at 0 and sampler lod bounds),
		// and min_lod_clamp without gradients is ignored (alone or with bias).
		if (_entryPoint->stage == Stage::Kernel || _entryPoint->stage == Stage::Vertex) {
			minLod = InvalidId;
			if (bias != InvalidId) {
				lod = bias;
				bias = InvalidId;
			}
		} else if (_entryPoint->stage != Stage::Fragment) {
			throw CompileError("min_lod_clamp and bias are lowered in a fragment, vertex or kernel function only");
		}
	}

	// Vulkan requires Lod 0 through an unnormalized sampler, whatever level() says.
	if (sampler.unnormalizedSampler) {
		lod = _builder.emitDeclTyped(spirv::OpConstant, floatType, { 0u });
	}

	const Id image = loadFrom(texture.id, texture.pointeeType);
	const Id loadedSampler = loadFrom(sampler.id, sampler.pointeeType);
	const Id combined = _builder.emitTyped(spirv::OpSampledImage,
		_types.sampledImage(texture.pointeeMsl.resource, textureSampledKind(texture.pointeeMsl.scalar), texture.pointeeMsl.isDepth, false),
		{ image, loadedSampler });

	const Id float4 = _types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4);
	const bool noOption = lod == InvalidId && bias == InvalidId && gradientX == InvalidId;
	if (noOption && minLod == InvalidId && library && !sampler.unnormalizedSampler) {
		throw StageError("a sample with no explicit level in a library helper: whether it is implicit "
			"depends on the stage of the entry point that calls it, which a library does not know; "
			"pass level(...)");
	}
	const bool implicit = (noOption && _entryPoint->stage == Stage::Fragment && !sampler.unnormalizedSampler)
		|| bias != InvalidId || (minLod != InvalidId && gradientX == InvalidId);
	if (noOption && !implicit) {
		lod = _builder.emitDeclTyped(spirv::OpConstant, floatType, { 0u });
	}
	uint32_t mask = 0;
	std::vector<uint32_t> words { combined, coordinate };
	if (compare) {
		words.push_back(reference);
	}
	const size_t maskAt = words.size();
	words.push_back(0);
	if (bias != InvalidId) {
		mask |= 0x1;
		words.push_back(bias);
	}
	if (lod != InvalidId) {
		mask |= kImageOperandsLod;
		words.push_back(lod);
	}
	if (gradientX != InvalidId) {
		mask |= 0x4;  // Grad
		words.push_back(gradientX);
		words.push_back(gradientY);
	}
	if (offset != InvalidId) {
		mask |= 0x8;  // ConstOffset
		words.push_back(offset);
	}
	if (minLod != InvalidId) {
		mask |= 0x80;  // MinLod
		words.push_back(minLod);
		if (!_minLodDeclared) {
			_minLodDeclared = true;
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::MinLod) });
		}
	}
	if (mask == 0) {
		words.pop_back();
	} else {
		words[maskAt] = mask;
	}
	if (compare) {
		return _builder.emitTyped(implicit ? spirv::OpImageSampleDrefImplicitLod : spirv::OpImageSampleDrefExplicitLod,
			_types.scalar(ScalarKind::Float), words);
	}
	const Id sampled = _builder.emitTyped(implicit ? spirv::OpImageSampleImplicitLod : spirv::OpImageSampleExplicitLod,
		float4, words);
	return textureTexel(texture, sampled);
}

// read(uint2 coordinate [, lod]), a uint3 coordinate on a texture3d, or a uint2
// coordinate and a uint layer on a texture2d_array: a texel by integer
// coordinate, with no sampler, at the lod given. A storage image takes no lod. Metal
// takes uint and ushort vectors and rejects int and float ones.
Id Emitter::emitTextureRead(const Expression& call, const Binding& texture) {
	const auto& arguments = call.arguments;
	const ResourceKind kind = texture.pointeeMsl.resource;
	const bool storage = texture.pointeeMsl.isStorageTexture();
	if (kind == ResourceKind::TextureBuffer && arguments.size() != 1) {
		throw CompileError("read on a texture_buffer takes one coordinate: a texture_buffer has no mip levels");
	}
	// A storage image or a fetched cube takes a cube's face (and layer) as a layer of its own.
	const bool faces = storage || texture.pointeeMsl.isFetchedCube();
	const size_t layers = faces ? layerArguments(kind) : isArrayKind(kind) ? 1 : 0;
	const bool arrayed = layers > 0;
	const size_t fixed = 1 + layers;
	if (storage && arguments.size() == fixed + 1) {
		throw CompileError(std::string("mip arguments are unsupported on access::")
			+ textureAccessName(texture.pointeeMsl.textureAccess) + " textures: Apple accepts the level, but a Vulkan "
			"storage image view is one level, so the host binds it; read takes only a coordinate");
	}
	if (arguments.size() < fixed || arguments.size() > fixed + 1) {
		throw CompileError(std::string("read takes a coordinate")
			+ (arrayed ? ", an array index" : "") + " and optionally a lod; this call passes "
			+ std::to_string(arguments.size()));
	}

	const uint32_t width = faces ? storageCoordinateWidth(kind) : textureCoordinateWidth(kind);
	Id coordinate = emitExpression(*arguments[0]);
	const Id coordinateType = _builder.typeOf(coordinate);
	const bool unsignedVector = _types.vectorWidth(coordinateType) == width
		&& !_types.isFloat(coordinateType) && !_types.isSignedInt(coordinateType)
		&& (_types.bitWidth(coordinateType) == 32 || _types.bitWidth(coordinateType) == 16);
	if (!unsignedVector) {
		const std::string suffix = width == 1 ? std::string() : std::to_string(width);
		throw CompileError("the coordinate of read has to be a uint" + suffix + " or a ushort" + suffix);
	}
	coordinate = convert(coordinate, coordinateType, _types.vector(ScalarKind::UInt, width));
	coordinate = appendLayers(coordinate, faces ? kind : arrayed ? ResourceKind::Texture2DArray : ResourceKind::Texture2D,
		arguments, 1, "read");

	if (storage) {
		const Id storageImage = loadFrom(texture.id, texture.pointeeType);
		return texturePixels(texture, _builder.emitTyped(spirv::OpImageRead,
			_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4), { storageImage, coordinate }));
	}

	if (kind == ResourceKind::TextureBuffer) {
		// Dim Buffer takes no Lod operand.
		return texturePixels(texture, _builder.emitTyped(spirv::OpImageFetch,
			_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4),
			{ loadFrom(texture.id, texture.pointeeType), coordinate }));
	}
	if (isOneDimensional(kind) && arguments.size() == fixed + 1) {
		requireTexture1DLevelZero(*arguments[fixed], "read");
	}
	Id lod = arguments.size() == fixed + 1 && !isOneDimensional(kind)
		? emitLod(*arguments[fixed], "read") : constantU32(0);

	const Id image = loadFrom(texture.id, texture.pointeeType);
	const Id fetched = _builder.emitTyped(spirv::OpImageFetch,
		_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4), { image, coordinate, kImageOperandsLod, lod });
	return textureTexel(texture, fetched);
}

// The methods of a multisampled texture: read(coordinate [, array index], sample) is
// OpImageFetch with the Sample operand and no lod, get_num_samples is
// OpImageQuerySamples, and the size queries take no argument.
Id Emitter::emitMultisampledCall(const Expression& call, const Binding& texture) {
	const std::string& method = call.left->memberName;
	const auto& arguments = call.arguments;
	const ResourceKind kind = texture.pointeeMsl.resource;
	const bool arrayed = isArrayKind(kind);
	if (method == "get_width" || method == "get_height") {
		return emitTextureSize(call, texture, method == "get_width" ? SizeComponent::Width : SizeComponent::Height);
	}
	if (method == "get_array_size" && arrayed) {
		return emitTextureSize(call, texture, SizeComponent::ArraySize);
	}
	if (method == "get_num_samples") {
		if (!arguments.empty()) {
			throw CompileError("get_num_samples takes no arguments");
		}
		if (!_imageQueryDeclared) {
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageQuery) });
			_imageQueryDeclared = true;
		}
		return _builder.emitTyped(spirv::OpImageQuerySamples, _uintType, { loadFrom(texture.id, texture.pointeeType) });
	}
	if (method != "read") {
		throw CompileError("the " + typeName(texture.pointeeMsl) + " method \"" + method + "\" is not lowered yet; mslc lowers "
			"read, get_num_samples, get_width, get_height and get_array_size on a multisampled texture");
	}
	const size_t fixed = arrayed ? 3 : 2;
	if (arguments.size() != fixed) {
		throw CompileError(std::string("read on a multisampled texture takes a coordinate")
			+ (arrayed ? ", an array index" : "") + " and a sample index");
	}
	Id coordinate = emitExpression(*arguments[0]);
	const Id coordinateType = _builder.typeOf(coordinate);
	const bool unsignedPair = _types.vectorWidth(coordinateType) == 2 && !_types.isFloat(coordinateType)
		&& !_types.isSignedInt(coordinateType) && (_types.bitWidth(coordinateType) == 32 || _types.bitWidth(coordinateType) == 16);
	if (!unsignedPair) {
		throw CompileError("the coordinate of read has to be a uint2 or a ushort2");
	}
	coordinate = convert(coordinate, coordinateType, _types.vector(ScalarKind::UInt, 2));
	if (arrayed) {
		coordinate = appendArrayLayer(coordinate, ScalarKind::UInt, *arguments[1], "read");
	}
	const Id sampleIndex = layerIndex(*arguments[fixed - 1], "read", "sample index");
	const Id image = loadFrom(texture.id, texture.pointeeType);
	return textureTexel(texture, _builder.emitTyped(spirv::OpImageFetch,
		_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4),
		{ image, coordinate, 0x40 /* Sample */, sampleIndex }));
}

// get_width(), get_height() and get_depth() or get_array_size() (component 0,
// 1 and 2 of the size), of level 0 or of the lod given.
Id Emitter::emitTextureSize(const Expression& call, const Binding& texture, SizeComponent component) {
	const std::string& name = call.left->memberName;
	const bool buffer = texture.pointeeMsl.resource == ResourceKind::TextureBuffer;
	const bool storage = texture.pointeeMsl.isStorageTexture() || texture.pointeeMsl.isMultisampled || buffer;
	if (storage && !call.arguments.empty()) {
		throw CompileError(buffer ? name + " takes no arguments: a texture_buffer has no mip levels"
			: texture.pointeeMsl.isMultisampled
			? name + " takes no arguments: a multisampled texture has no mip levels"
			: std::string("mip arguments are unsupported on access::")
				+ textureAccessName(texture.pointeeMsl.textureAccess) + " textures: Apple accepts the level, but a Vulkan "
				"storage image view is one level, so the host binds it; " + name + " takes no arguments");
	}
	if (call.arguments.size() > 1) {
		throw CompileError(name + " takes an optional lod; this call passes "
			+ std::to_string(call.arguments.size()));
	}

	const bool oneLevel = isOneDimensional(texture.pointeeMsl.resource);
	if (oneLevel && !call.arguments.empty()) {
		requireTexture1DLevelZero(*call.arguments[0], name);
	}
	const Id lod = storage ? InvalidId
		: call.arguments.empty() || oneLevel ? constantU32(0) : emitLod(*call.arguments[0], name);

	if (!_imageQueryDeclared) {
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageQuery) });
		_imageQueryDeclared = true;
	}

	const Id image = loadFrom(texture.id, texture.pointeeType);
	// A fetched cube is the 2D array of its faces, so its size has their count as a third component.
	const bool cubeFaces = texture.pointeeMsl.isFetchedCube();
	const Id size = _builder.emitTyped(storage ? spirv::OpImageQuerySize : spirv::OpImageQuerySizeLod,
		_types.vector(ScalarKind::UInt, cubeFaces ? 3 : textureSizeWidth(texture.pointeeMsl.resource)),
		storage ? std::vector<uint32_t>{ image } : std::vector<uint32_t>{ image, lod });
	if (cubeFaces && component == SizeComponent::ArraySize) {
		return _builder.emitTyped(spirv::OpUDiv, _uintType,
			{ _builder.emitTyped(spirv::OpCompositeExtract, _uintType, { size, 2u }), constantU32(6) });
	}
	if (texture.pointeeMsl.resource == ResourceKind::Texture1D || buffer) {
		return size;
	}
	const uint32_t index = component == SizeComponent::ArraySize && texture.pointeeMsl.resource == ResourceKind::Texture1DArray
		? 1u : static_cast<uint32_t>(component);
	return _builder.emitTyped(spirv::OpCompositeExtract, _uintType, { size, index });
}

// A mip level argument: any number, converted to the uint the instruction takes.
Id Emitter::emitLod(const Expression& expression, const std::string& callName) {
	const Id value = emitNonComponent(expression, callName);
	const Id type = _builder.typeOf(value);
	if (_types.vectorWidth(type) != 1 || _types.bitWidth(type) < 8) {
		throw CompileError("the lod of " + callName + " has to be a number");
	}

	return convert(value, type, _uintType);
}

} // namespace mslc
