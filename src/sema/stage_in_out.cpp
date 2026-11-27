#include "sema/emitter.h"

namespace mslc {

// Each buffer's address is loaded once, here, so that the value dominates
// every use. A load emitted at the first use would not dominate a later use
// in another basic block, and spirv-val rejects a module where an id is used
// outside the block that defined it.
void Emitter::preloadBufferBases() {
	for (size_t k = 0; k < _bufferMembers.size(); ++k) {
		const Id memberType = _types.bufferPointer(_bufferMembers[k]);
		const Id memberAddress = _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(spirv::StorageClass::Uniform, memberType),
			{ _addressBlock.variable, constantU32(static_cast<uint32_t>(k)) });
		Id base = _builder.emitTyped(spirv::OpLoad, memberType, { memberAddress });
		const auto matrixType = _bufferMatrixTypes.find(static_cast<uint32_t>(k));
		if (matrixType != _bufferMatrixTypes.end()) {
			// The base of the reference is the matrix inside its wrapper struct.
			base = _builder.emitTyped(spirv::OpAccessChain,
				_types.pointer(spirv::StorageClass::PhysicalStorageBuffer, matrixType->second),
				{ base, constantU32(0) });
		}
		_bufferBases.emplace(static_cast<uint32_t>(k), base);
		const auto boolType = _bufferBoolTypes.find(static_cast<uint32_t>(k));
		if (boolType != _bufferBoolTypes.end()) {
			boolAddress(base, boolType->second);
		}
	}
}

// The struct a type names when it is a struct value, and not a pointer to one
// or an array of them.
const StructDecl* Emitter::structValue(const Type& type) const {
	return type.isPointer || type.arrayLength ? nullptr : _unit.findStruct(type.namedType);
}

// A half crosses as a float: Vulkan's shaderFloat16 does not cover the Input
// and Output storage classes, which need storageInputOutput16, a feature a
// device need not have, and widening a half is exact. Narrower integers,
// 64-bit values, bools and anything wider than a vector are reported rather
// than given a layout guessed at.
StageVariable Emitter::declareStageVariable(const Type& type,
	spirv::StorageClassValue storageClass, const std::string& what) {

	if (type.isPacked) {
		throw CompileError(what + " is a " + typeName(type) + ", which Apple's compiler does not "
			"allow across a stage boundary; use " + typeName(Type { type.scalar, type.vectorWidth }));
	}

	// A bool crosses from a vertex function to a fragment function as an unsigned
	// integer that is 0 or 1; Apple refuses it as a colour output, and a bool
	// vertex attribute has no layout here.
	const bool crossesStages = (_entryPoint->stage == Stage::Vertex && storageClass == spirv::StorageClass::Output)
		|| (_entryPoint->stage == Stage::Fragment && storageClass == spirv::StorageClass::Input);
	const bool carriedBool = type.scalar == ScalarKind::Bool && crossesStages;
	// A char or a short is stored in 32 bits across the boundary, as a half is in a
	// float; Apple accepts them there and refuses 64-bit integers.
	const bool narrowInteger = crossesStages && (type.scalar == ScalarKind::Char || type.scalar == ScalarKind::UChar
		|| type.scalar == ScalarKind::Short || type.scalar == ScalarKind::UShort);

	const uint32_t bits = scalarBitWidth(type.scalar);
	if (type.isPointer || type.arrayLength || !type.namedType.empty() || type.isMatrix()
		|| (type.scalar == ScalarKind::Bool && !carriedBool)
		|| (bits != 32 && type.scalar != ScalarKind::Half && !carriedBool && !narrowInteger)) {
		throw CompileError(what + " is a " + typeName(type) + ", which mslc does not pass "
			"between stages yet; it passes a scalar or a vector of 32-bit components or of half");
	}

	Type carried = type;
	if (carried.scalar == ScalarKind::Half) {
		carried.scalar = ScalarKind::Float;
	}
	if (carriedBool) {
		carried.scalar = ScalarKind::UInt;
	}
	if (narrowInteger) {
		carried.scalar = type.scalar == ScalarKind::Char || type.scalar == ScalarKind::Short
			? ScalarKind::Int : ScalarKind::UInt;
	}

	StageVariable stage;
	stage.valueType = declaredTypeOf(type);
	stage.interfaceType = declaredTypeOf(carried);
	stage.variable = _builder.emitDeclTyped(spirv::OpVariable,
		_types.pointer(storageClass, stage.interfaceType), { static_cast<uint32_t>(storageClass) });
	_interface.push_back(stage.variable);
	return stage;
}

// An interpolation attribute as SPIR-V decorations. center_perspective is
// Vulkan's default and decorates nothing; the centroid and sample forms add
// Centroid or Sample, and the no_perspective forms add NoPerspective. Sample
// needs the SampleRateShading capability.
void Emitter::decorateInterpolation(Id variable, Interpolation interpolation) {
	const auto decorate = [&](spirv::DecorationValue decoration) {
		_builder.emit(spirv::OpDecorate, { variable, static_cast<uint32_t>(decoration) });
	};

	switch (interpolation) {
		case Interpolation::None:
		case Interpolation::CenterPerspective:
			return;
		case Interpolation::Flat:
			decorate(spirv::Decoration::Flat);
			return;
		case Interpolation::CenterNoPerspective:
			decorate(spirv::Decoration::NoPerspective);
			return;
		case Interpolation::CentroidPerspective:
			decorate(spirv::Decoration::Centroid);
			return;
		case Interpolation::CentroidNoPerspective:
			decorate(spirv::Decoration::Centroid);
			decorate(spirv::Decoration::NoPerspective);
			return;
		case Interpolation::SamplePerspective:
		case Interpolation::SampleNoPerspective:
			if (!_sampleRateShadingDeclared) {
				_builder.emit(spirv::OpCapability,
					{ static_cast<uint32_t>(spirv::Capability::SampleRateShading) });
				_sampleRateShadingDeclared = true;
			}
			decorate(spirv::Decoration::Sample);
			if (interpolation == Interpolation::SampleNoPerspective) {
				decorate(spirv::Decoration::NoPerspective);
			}
			return;
	}
}

// The "locnN" a [[user(...)]] name spells, N being the Location it gives. Only
// the canonical decimal spelling counts, so locn01 is a plain name, as it is a
// different name from locn1 on Apple; two fields cannot then claim one Location
// without repeating a name.
static std::optional<uint32_t> locnLocation(const std::string& name) {
	constexpr size_t prefix = 4;
	constexpr size_t maxDigits = 9;
	if (name.compare(0, prefix, "locn") != 0 || name.size() == prefix) {
		return std::nullopt;
	}
	const std::string digits = name.substr(prefix);
	if (digits.find_first_not_of("0123456789") != std::string::npos
		|| (digits.size() > 1 && digits[0] == '0')) {
		return std::nullopt;
	}
	if (digits.size() > maxDigits) {
		throw CompileError("[[user(" + name + ")]] names a Location too large to be one");
	}
	return static_cast<uint32_t>(std::stoul(digits));
}

// The Location of each field of a struct that crosses from the vertex to the
// fragment stage, nullopt for [[position]]. A [[user(locnN)]] field is at N;
// every other field takes the first Location no locnN field of the struct
// claims, in declaration order. A module sees only its own struct, so a
// Location has to follow from the struct alone: a name that is not locnN says
// nothing about where its partner stands in the other stage.
std::vector<std::optional<uint32_t>> stageLocations(const StructDecl& decl) {
	std::set<uint32_t> claimed;
	std::set<std::string> names;
	for (const StructField& field: decl.fields) {
		if (!field.attributes.userName) {
			continue;
		}
		if (!names.insert(*field.attributes.userName).second) {
			throw CompileError("duplicated user-defined name \"" + *field.attributes.userName
				+ "\" in struct \"" + decl.name + "\"");
		}
		const auto location = field.attributes.position
			? std::nullopt : locnLocation(*field.attributes.userName);
		if (location) {
			claimed.insert(*location);
		}
	}

	std::vector<std::optional<uint32_t>> locations;
	uint32_t next = 0;
	for (const StructField& field: decl.fields) {
		if (field.attributes.position || field.attributes.colorIndex) {
			locations.emplace_back(std::nullopt);
		} else if (const auto named = field.attributes.userName
			? locnLocation(*field.attributes.userName) : std::nullopt) {
			locations.emplace_back(*named);
		} else if (field.attributes.pointSize) {
			locations.emplace_back(std::nullopt);
		} else {
			while (claimed.count(next) != 0) {
				++next;
			}
			locations.emplace_back(next++);
		}
	}
	return locations;
}

// One variable per field. The [[position]] field is a builtin, Position on a
// vertex output and FragCoord on a fragment input; every other field takes the
// next Location in declaration order (stageLocations), which is how Iridium numbers them on
// both sides (indium src/iridium/air.cpp: the struct return loop and the
// air.fragment_input case), so the two stages agree when they share a struct.
std::vector<StageVariable> Emitter::declareStageStruct(const StructDecl& decl,
	spirv::StorageClassValue storageClass) {

	const bool isInput = storageClass == spirv::StorageClass::Input;
	std::vector<StageVariable> variables;
	const std::vector<std::optional<uint32_t>> locations = stageLocations(decl);
	bool hasPosition = false;
	bool hasPointSize = false;

	for (size_t index = 0; index < decl.fields.size(); ++index) {
		const StructField& field = decl.fields[index];
		const std::string what = "field \"" + field.name + "\" of \"" + decl.name + "\"";

		if (field.attributes.invariant && (isInput || !field.attributes.position
			|| field.attributes.colorIndex || field.attributes.pointSize)) {
			throw CompileError(what + " has [[invariant]], which is only valid on a vertex output's [[position]] field");
		}
		if (field.attributes.depthMode) {
			throw CompileError(what + " has [[depth]], which is only valid on a fragment output");
		}
		if (field.attributes.stencil) {
			throw CompileError(what + " has [[stencil]], which is only valid on a fragment output");
		}
		if (field.attributes.sampleMask) {
			throw CompileError(isInput
				? what + " has [[sample_mask]] on a fragment input, which mslc does not lower yet"
				: what + " has [[sample_mask]], which is not valid on a vertex output");
		}
		if (field.attributes.colorIndex) {
			if (!isInput) {
				throw CompileError(what + " is [[color(n)]], which is not valid on a vertex output; it names a "
					"fragment output's colour attachment");
			}
			if (field.attributes.position || field.attributes.pointSize || field.attributes.userName
				|| field.attributes.attributeIndex || field.attributes.interpolation != Interpolation::None) {
				throw CompileError(what + " is [[color(n)]], which cannot also carry another attribute");
			}
			variables.push_back(declareFramebufferFetch(field.type, *field.attributes.colorIndex, field.name, what));
			continue;
		}
		const Interpolation interpolation = field.attributes.interpolation;
		if (field.attributes.position && interpolation != Interpolation::None) {
			throw CompileError(what + " is [[position]], which cannot also have an interpolation attribute");
		}
		if (field.attributes.pointSize) {
			if (isInput) {
				throw CompileError(what + " has [[point_size]] on a fragment input, which mslc does not lower yet");
			}
			if (field.attributes.position) {
				throw CompileError(what + " has an incompatible point_size attribute combination");
			}
			if (field.attributes.userName || interpolation != Interpolation::None) {
				throw CompileError(what + " combines point_size with user or interpolation, which mslc does not lower");
			}
			if (hasPointSize) {
				throw CompileError("struct \"" + decl.name + "\" has more than one [[point_size]] field");
			}
			const Type& type = field.type;
			if (type.scalar != ScalarKind::Float || !type.isScalar() || type.isPointer
				|| type.arrayLength || !type.namedType.empty()) {
				throw CompileError(what + " has [[point_size]], which has to be a scalar float");
			}
			hasPointSize = true;
		}

		if (!isInput && !field.type.enumName.empty()) {
			throw CompileError(what + " has the enum type " + field.type.enumName + ", which Apple's "
				"compiler does not allow in a vertex function's returned struct");
		}

		const StageVariable variable = declareStageVariable(field.type, storageClass, what);
		const bool integral = !_types.isFloat(variable.interfaceType);

		if (field.attributes.position) {
			if (hasPosition) {
				throw CompileError("struct \"" + decl.name + "\" has more than one [[position]] field");
			}
			if (variable.valueType != _types.vector(ScalarKind::Float, 4)) {
				throw CompileError(what + " is [[position]], which has to be a float4");
			}

			if (isInput) {
				if (_hasFragCoordInput) {
					throw CompileError(what + " is [[position]], which collides with this "
						"function's other fragment position input");
				}
				_hasFragCoordInput = true;
			}

			hasPosition = true;
			_builder.emit(spirv::OpDecorate, { variable.variable,
				static_cast<uint32_t>(spirv::Decoration::BuiltIn),
				static_cast<uint32_t>(isInput ? spirv::BuiltIn::FragCoord : spirv::BuiltIn::Position) });
			if (!isInput && field.attributes.invariant) {
				_builder.emit(spirv::OpDecorate, { variable.variable,
					static_cast<uint32_t>(spirv::Decoration::Invariant) });
			}
		} else if (field.attributes.pointSize) {
			_builder.emit(spirv::OpDecorate, { variable.variable,
				static_cast<uint32_t>(spirv::Decoration::BuiltIn),
				static_cast<uint32_t>(spirv::BuiltIn::PointSize) });
		} else {
			_builder.emit(spirv::OpDecorate, { variable.variable,
				static_cast<uint32_t>(spirv::Decoration::Location), *locations[index] });

			// The pairing of a vertex output with a fragment input is by name (see stageLocations), and
			// the two functions are usually compiled apart, so the host that links them needs each one's
			// names, types and Locations.
			addReflectionEntry(std::string("{ \"kind\": \"") + (isInput ? "StageInput" : "StageOutput")
				+ "\", \"location\": " + std::to_string(*locations[index]) + ", \"name\": \""
				+ field.attributes.userName.value_or(field.name) + "\", \"type\": \"" + typeName(field.type) + "\" }");

			if (integral && interpolation != Interpolation::None && interpolation != Interpolation::Flat) {
				throw CompileError(what + " is " + typeName(field.type) + ", which requires [[flat]]; "
					"an integer is not interpolated, and Apple's compiler rejects any other qualifier");
			}

			decorateInterpolation(variable.variable, interpolation);

			// VUID-StandaloneSpirv-Flat-04744: an integer fragment input is not
			// interpolated, and the module is rejected unless it says so.
			if (isInput && integral && interpolation == Interpolation::None) {
				_builder.emit(spirv::OpDecorate, { variable.variable,
					static_cast<uint32_t>(spirv::Decoration::Flat) });
			}
		}

		variables.push_back(variable);
	}

	// Apple rejects a vertex function whose returned struct has no position
	// ("invalid return type"), and a rasteriser has nothing to place without it.
	if (!isInput && !hasPosition) {
		throw CompileError("struct \"" + decl.name + "\" is returned by a vertex function and "
			"has no [[position]] field");
	}

	return variables;
}

// A vertex function's [[stage_in]] struct: one Input per field, at the Location
// the field's [[attribute(n)]] names. Iridium decorates an air.vertex_input
// parameter with its air.location_index (indium src/iridium/air.cpp:652-660)
// and indium builds the pipeline's VkVertexInputAttributeDescription with
// location = the vertex descriptor's attribute index
// (src/indium/render-pipeline.cpp:114), so the Location is n, not the field's
// position in the struct. The reflection lists each so a consumer can see
// which attributes the function reads.
std::vector<StageVariable> Emitter::declareVertexAttributes(const StructDecl& decl) {
	std::vector<StageVariable> variables;
	std::set<uint32_t> used;

	if (decl.fields.empty()) {
		throw CompileError("[[stage_in]] struct \"" + decl.name + "\" of vertex function \""
			+ _entryPoint->name + "\" has no fields");
	}

	for (const StructField& field: decl.fields) {
		const std::string what = "field \"" + field.name + "\" of \"" + decl.name + "\"";
		const Type& type = field.type;

		if (field.attributes.invariant) {
			throw CompileError(what + " has [[invariant]], which is only valid on a vertex output's [[position]] field");
		}
		if (field.attributes.depthMode) {
			throw CompileError(what + " has [[depth]], which is only valid on a fragment output");
		}
		if (field.attributes.stencil) {
			throw CompileError(what + " has [[stencil]], which is only valid on a fragment output");
		}
		if (field.attributes.sampleMask) {
			throw CompileError(what + " has [[sample_mask]], which is not valid on a vertex input");
		}
		if (field.attributes.position) {
			throw CompileError(what + " is [[position]], which a vertex function's [[stage_in]] "
				"struct cannot carry");
		}
		if (field.attributes.pointSize) {
			throw CompileError(what + " has [[point_size]], which a vertex function's [[stage_in]] "
				"struct cannot carry");
		}
		if (field.attributes.interpolation != Interpolation::None || field.attributes.colorIndex) {
			throw CompileError(what + " has an interpolation or colour attribute, which is not "
				"valid on a vertex attribute; an interpolation attribute belongs on a field "
				"that crosses to the fragment stage");
		}
		if (!field.attributes.attributeIndex) {
			throw CompileError(what + " has no [[attribute(n)]]; every field of a vertex "
				"function's [[stage_in]] struct needs one");
		}
		const uint32_t index = *field.attributes.attributeIndex;
		if (!used.insert(index).second) {
			throw CompileError(what + " reuses [[attribute(" + std::to_string(index) + ")]]");
		}

		if (type.isPointer || !type.namedType.empty() || type.isMatrix() || type.isPacked) {
			throw CompileError(what + " is a " + typeName(type) + ", which Apple's compiler does "
				"not allow as a vertex attribute; use a scalar or a vector");
		}
		if (type.scalar != ScalarKind::Half && scalarBitWidth(type.scalar) != 32
			&& type.scalar != ScalarKind::Bool) {
			throw CompileError(what + " is a " + typeName(type) + ", which mslc does not lower as "
				"a vertex attribute yet; use a 32-bit scalar or vector, or half");
		}
		if (type.scalar == ScalarKind::Bool) {
			throw CompileError(what + " is a " + typeName(type) + ", which mslc does not lower as "
				"a vertex attribute; use an int or a uint");
		}

		variables.push_back(declareStageVariable(type, spirv::StorageClass::Input,
			"vertex attribute " + what));
		_builder.emit(spirv::OpDecorate, { variables.back().variable,
			static_cast<uint32_t>(spirv::Decoration::Location), index });
		addReflectionEntry("{ \"kind\": \"VertexInput\", \"metal_index\": "
			+ std::to_string(index) + ", \"location\": " + std::to_string(index)
			+ ", \"name\": \"" + field.name + "\" }");
	}

	return variables;
}

std::vector<StageVariable> Emitter::declareFragmentOutputs(const StructDecl& decl) {
	constexpr uint32_t maxColorAttachments = 8;
	std::vector<StageVariable> variables;
	std::set<uint32_t> used;
	bool hasDepth = false;
	bool hasSampleMask = false;
	bool hasStencil = false;

	for (const StructField& field: decl.fields) {
		const std::string what = "field \"" + field.name + "\" of \"" + decl.name + "\"";

		if (field.attributes.invariant) {
			throw CompileError(what + " has [[invariant]], which is only valid on a vertex output's [[position]] field");
		}
		if (field.attributes.stencil) {
			// The stencil reference value, which Vulkan exports with SPV_EXT_shader_stencil_export.
			if (field.attributes.colorIndex || field.attributes.position || field.attributes.pointSize
				|| field.attributes.sampleMask || field.attributes.depthMode) {
				throw CompileError(what + " has an incompatible stencil attribute combination");
			}
			if (field.attributes.attributeIndex || field.attributes.userName
				|| field.attributes.interpolation != Interpolation::None) {
				throw CompileError(what + " combines stencil with an attribute index, user or interpolation, "
					"which mslc does not lower");
			}
			if (hasStencil) {
				throw CompileError("struct \"" + decl.name + "\" has more than one [[stencil]] field");
			}
			const Type& type = field.type;
			if (type.scalar != ScalarKind::UInt || !type.isScalar() || type.isPointer
				|| type.arrayLength || !type.namedType.empty()) {
				throw CompileError(what + " has [[stencil]], which has to be a scalar uint");
			}
			hasStencil = true;
			if (!_stencilExportDeclared) {
				_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::StencilExportEXT) });
				std::vector<uint32_t> extension;
				spirv::Builder::appendString(extension, "SPV_EXT_shader_stencil_export");
				_builder.emit(spirv::OpExtension, extension);
				_stencilExportDeclared = true;
			}
			variables.push_back(declareStageVariable(type, spirv::StorageClass::Output, what));
			_builder.emit(spirv::OpDecorate, { variables.back().variable,
				static_cast<uint32_t>(spirv::Decoration::BuiltIn),
				static_cast<uint32_t>(spirv::BuiltIn::FragStencilRefEXT) });
			continue;
		}
		if (field.attributes.depthMode) {
			if (field.attributes.colorIndex || field.attributes.position || field.attributes.pointSize
				|| field.attributes.sampleMask) {
				throw CompileError(what + " has an incompatible depth attribute combination");
			}
			if (field.attributes.attributeIndex) {
				throw CompileError(what + " combines depth with an attribute index, which mslc does not lower");
			}
			if (field.attributes.userName || field.attributes.interpolation != Interpolation::None) {
				throw CompileError(what + " combines depth with user or interpolation, which mslc does not lower");
			}
			if (hasDepth) {
				throw CompileError("struct \"" + decl.name + "\" has more than one [[depth]] field");
			}
			const Type& type = field.type;
			if (type.scalar != ScalarKind::Float || !type.isScalar() || type.isPointer
				|| type.arrayLength || !type.namedType.empty()) {
				throw CompileError(what + " has [[depth]], which has to be a scalar float");
			}
			hasDepth = true;
			variables.push_back(declareStageVariable(type, spirv::StorageClass::Output, what));
			_builder.emit(spirv::OpDecorate, { variables.back().variable,
				static_cast<uint32_t>(spirv::Decoration::BuiltIn),
				static_cast<uint32_t>(spirv::BuiltIn::FragDepth) });
			continue;
		}
		if (field.attributes.sampleMask) {
			if (field.attributes.colorIndex || field.attributes.position || field.attributes.pointSize) {
				throw CompileError(what + " has an incompatible sample_mask attribute combination");
			}
			if (field.attributes.attributeIndex) {
				throw CompileError(what + " combines sample_mask with an attribute index, which mslc does not lower");
			}
			if (field.attributes.userName || field.attributes.interpolation != Interpolation::None) {
				throw CompileError(what + " combines sample_mask with user or interpolation, which mslc does not lower");
			}
			if (hasSampleMask) {
				throw CompileError("struct \"" + decl.name + "\" has more than one [[sample_mask]] field");
			}
			const Type& type = field.type;
			if (type.scalar != ScalarKind::UInt || !type.isScalar() || type.isPointer
				|| type.arrayLength || !type.namedType.empty()) {
				throw CompileError(what + " has [[sample_mask]], which has to be a scalar uint");
			}
			hasSampleMask = true;
			const Id array = _types.packedStorage(ScalarKind::UInt, 1);
			const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
				_types.pointer(spirv::StorageClass::Output, array),
				{ static_cast<uint32_t>(spirv::StorageClass::Output) });
			_interface.push_back(variable);
			variables.push_back({ variable, array, _types.scalar(ScalarKind::UInt) });
			_builder.emit(spirv::OpDecorate, { variable,
				static_cast<uint32_t>(spirv::Decoration::BuiltIn),
				static_cast<uint32_t>(spirv::BuiltIn::SampleMask) });
			continue;
		}
		if (field.attributes.interpolation != Interpolation::None) {
			throw CompileError(what + " has an interpolation attribute, which is not valid on a "
				"fragment output");
		}
		if (field.attributes.position || field.attributes.attributeIndex) {
			throw CompileError(what + " has [[position]] or [[attribute(n)]], which is not valid on "
				"a fragment output");
		}
		if (field.attributes.pointSize) {
			throw CompileError(what + " has [[point_size]], which is only valid on a vertex output");
		}
		if (!field.attributes.colorIndex) {
			throw CompileError(what + " has no [[color(n)]], [[depth(mode)]], [[sample_mask]] or [[stencil]]; every field of a "
				"fragment function's returned struct needs one");
		}

		const uint32_t index = *field.attributes.colorIndex;
		if (index >= maxColorAttachments) {
			throw CompileError(what + " is [[color(" + std::to_string(index) + ")]], and Apple's "
				"compiler allows indices 0 to 7");
		}
		if (!used.insert(index).second) {
			throw CompileError(what + " reuses [[color(" + std::to_string(index) + ")]]");
		}

		Type carried = field.type;
		if ((carried.scalar == ScalarKind::Short || carried.scalar == ScalarKind::UShort)
			&& carried.vectorWidth <= 4 && !carried.isPointer && !carried.arrayLength && !carried.isPacked) {
			carried.scalar = carried.scalar == ScalarKind::Short ? ScalarKind::Int : ScalarKind::UInt;
		}
		StageVariable variable = declareStageVariable(carried, spirv::StorageClass::Output, what);
		variable.valueType = declaredTypeOf(field.type);
		variables.push_back(variable);
		_builder.emit(spirv::OpDecorate, { variables.back().variable,
			static_cast<uint32_t>(spirv::Decoration::Location), index });
	}

	return variables;
}

} // namespace mslc
