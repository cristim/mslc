#include "ast.h"

namespace mslc {

const char* scalarKindName(ScalarKind kind) {
	switch (kind) {
		case ScalarKind::Void: return "void";
		case ScalarKind::Bool: return "bool";
		case ScalarKind::Char: return "char";
		case ScalarKind::UChar: return "uchar";
		case ScalarKind::Short: return "short";
		case ScalarKind::UShort: return "ushort";
		case ScalarKind::Int: return "int";
		case ScalarKind::UInt: return "uint";
		case ScalarKind::Long: return "long";
		case ScalarKind::ULong: return "ulong";
		case ScalarKind::Half: return "half";
		case ScalarKind::Float: return "float";
		case ScalarKind::Double: return "double";
	}

	return "unknown";
}

const char* resourceKindName(ResourceKind kind) {
	switch (kind) {
		case ResourceKind::None: return "none";
		case ResourceKind::Texture1D: return "texture1d";
		case ResourceKind::Texture2D: return "texture2d";
		case ResourceKind::TextureCube: return "texturecube";
		case ResourceKind::Texture3D: return "texture3d";
		case ResourceKind::Texture2DArray: return "texture2d_array";
		case ResourceKind::Texture1DArray: return "texture1d_array";
		case ResourceKind::TextureCubeArray: return "texturecube_array";
		case ResourceKind::TextureBuffer: return "texture_buffer";
		case ResourceKind::Sampler: return "sampler";
	}

	return "unknown";
}

ScalarKind textureSampledKind(ScalarKind element) {
	switch (element) {
		case ScalarKind::Short: return ScalarKind::Int;
		case ScalarKind::UShort: return ScalarKind::UInt;
		case ScalarKind::Half: return ScalarKind::Float;
		default: return element;
	}
}

const char* textureAccessName(TextureAccess access) {
	switch (access) {
		case TextureAccess::Sample: return "sample";
		case TextureAccess::Write: return "write";
		case TextureAccess::Read: return "read";
		case TextureAccess::ReadWrite: return "read_write";
	}

	return "unknown";
}

std::string typeName(const Type& type) {
	if (type.resource == ResourceKind::Sampler) {
		return "sampler";
	}
	if (type.isTexture()) {
		std::string kindName = !type.isDepth ? std::string(resourceKindName(type.resource))
			: type.resource == ResourceKind::Texture2D ? "depth2d" : type.resource == ResourceKind::Texture2DArray ? "depth2d_array"
			: type.resource == ResourceKind::TextureCube ? "depthcube" : "depthcube_array";
		if (type.isMultisampled) {
			const size_t array = kindName.find("_array");
			kindName = array == std::string::npos ? kindName + "_ms" : kindName.substr(0, array) + "_ms_array";
		}
		// A function template's parameter still to be deduced, "<T>", is spelled as its name.
		const std::string sampled = type.namedType.size() > 2 && type.namedType.front() == '<'
			? type.namedType.substr(1, type.namedType.size() - 2) : std::string(scalarKindName(type.scalar));
		return kindName + "<" + sampled
			+ (type.textureAccess == TextureAccess::Sample ? std::string(">")
				: std::string(", access::") + textureAccessName(type.textureAccess) + ">");
	}

	std::string name = type.isAuto ? std::string("auto")
		: type.namedType.empty() ? std::string(scalarKindName(type.scalar))
		: type.namedType;

	if (type.namedType.empty() && type.isMatrix()) {
		name += std::to_string(type.matrixColumns) + "x" + std::to_string(type.vectorWidth);
	} else if (type.namedType.empty() && type.vectorWidth > 1) {
		name += std::to_string(type.vectorWidth);
		if (type.isPacked) {
			name = "packed_" + name;
		}
	} else if (type.vectorWidth > 1 && type.namedType.size() > 2 && type.namedType.front() == '<'
		&& type.namedType.back() == '>') {
		// vec<T, N> of a template parameter still to deduce, whose marker is "<T>".
		name = "vec<" + type.namedType.substr(1, type.namedType.size() - 2) + ", " + std::to_string(type.vectorWidth) + ">";
	}
	if (type.isAtomic) {
		name = "atomic_" + name;
	}
	if (type.arrayLength) {
		name += "[" + std::to_string(*type.arrayLength) + "]";
	}
	for (const uint32_t inner: type.innerArrayLengths) {
		name += "[" + std::to_string(inner) + "]";
	}
	if (type.isPointer) {
		name += "*";
	}
	if (type.isConst) {
		name = "const " + name;
	}

	return name;
}

const char* samplerAddressName(SamplerAddress mode) {
	switch (mode) {
		case SamplerAddress::ClampToZero: return "ClampToZero";
		case SamplerAddress::ClampToEdge: return "ClampToEdge";
		case SamplerAddress::Repeat: return "Repeat";
		case SamplerAddress::MirroredRepeat: return "MirrorRepeat";
	}

	return "unknown";
}

const char* samplerFilterName(SamplerFilter filter) {
	return filter == SamplerFilter::Linear ? "Linear" : "Nearest";
}

const char* samplerMipFilterName(SamplerMipFilter filter) {
	switch (filter) {
		case SamplerMipFilter::None: return "None";
		case SamplerMipFilter::Nearest: return "Nearest";
		case SamplerMipFilter::Linear: return "Linear";
	}

	return "unknown";
}

const char* samplerCompareName(SamplerCompare compare) {
	switch (compare) {
		case SamplerCompare::Never: return "Never";
		case SamplerCompare::Less: return "Less";
		case SamplerCompare::LessEqual: return "LessEqual";
		case SamplerCompare::Greater: return "Greater";
		case SamplerCompare::GreaterEqual: return "GreaterEqual";
		case SamplerCompare::Equal: return "Equal";
		case SamplerCompare::NotEqual: return "NotEqual";
		case SamplerCompare::Always: return "Always";
	}

	return "unknown";
}

const char* samplerBorderName(SamplerBorder border) {
	switch (border) {
		case SamplerBorder::TransparentBlack: return "TransparentBlack";
		case SamplerBorder::OpaqueBlack: return "OpaqueBlack";
		case SamplerBorder::OpaqueWhite: return "OpaqueWhite";
	}

	return "unknown";
}

const char* addressSpaceName(AddressSpace space) {
	switch (space) {
		case AddressSpace::Device: return "device";
		case AddressSpace::Constant: return "constant";
		case AddressSpace::Threadgroup: return "threadgroup";
		case AddressSpace::Thread: return "thread";
		case AddressSpace::None: break;
	}

	return "none";
}

uint32_t scalarBitWidth(ScalarKind kind) {
	switch (kind) {
		case ScalarKind::Bool: return 1;
		case ScalarKind::Char:
		case ScalarKind::UChar: return 8;
		case ScalarKind::Short:
		case ScalarKind::UShort:
		case ScalarKind::Half: return 16;
		case ScalarKind::Int:
		case ScalarKind::UInt:
		case ScalarKind::Float: return 32;
		case ScalarKind::Long:
		case ScalarKind::ULong:
		case ScalarKind::Double: return 64;
		case ScalarKind::Void: return 0;
	}

	return 0;
}

const FunctionDecl* TranslationUnit::findFunction(const std::string& name) const {
	for (const auto& function: functions) {
		if (function.name == name) {
			return &function;
		}
	}

	return nullptr;
}

const StructDecl* TranslationUnit::findStruct(const std::string& name) const {
	for (const auto& decl: structs) {
		if (decl.name == name) {
			return &decl;
		}
	}

	return nullptr;
}

bool sameParameterTypes(const FunctionDecl& a, const FunctionDecl& b) {
	if (a.parameters.size() != b.parameters.size()) {
		return false;
	}

	for (size_t i = 0; i < a.parameters.size(); ++i) {
		const Type& x = a.parameters[i].type;
		const Type& y = b.parameters[i].type;
		// An array parameter is a pointer to its element whatever its length, and a const
		// on a parameter passed by value is not part of the signature; on a pointer or a
		// reference, it and the address space are.
		const bool pointerX = x.isPointer || x.arrayLength.has_value();
		const bool pointerY = y.isPointer || y.arrayLength.has_value();
		const bool referenceX = a.parameters[i].isReference;
		if (referenceX != b.parameters[i].isReference || pointerX != pointerY
			|| x.innerArrayLengths != y.innerArrayLengths) {
			return false;
		}
		if ((pointerX || referenceX) && (x.isConst != y.isConst || x.addressSpace != y.addressSpace)) {
			return false;
		}
		if (x.scalar != y.scalar || x.vectorWidth != y.vectorWidth || x.matrixColumns != y.matrixColumns
			|| x.isPacked != y.isPacked || x.namedType != y.namedType || x.enumName != y.enumName
			|| x.isAtomic != y.isAtomic
			|| x.resource != y.resource || x.textureAccess != y.textureAccess
			|| x.isDepth != y.isDepth || x.isMultisampled != y.isMultisampled) {
			return false;
		}
	}

	return true;
}

std::string parameterSignature(const Parameter& parameter) {
	Type type = parameter.type;
	const bool pointer = type.isPointer || type.arrayLength.has_value();
	if (pointer) {
		type.arrayLength.reset();
		type.isPointer = true;
	}
	if (!pointer && !parameter.isReference) {
		type.isConst = false;
	}
	if (!type.enumName.empty()) {
		type.namedType = type.enumName;
	}
	std::string name = typeName(type);
	if ((pointer || parameter.isReference) && type.addressSpace != AddressSpace::None) {
		name = std::string(addressSpaceName(type.addressSpace)) + " " + name;
	}
	return parameter.isReference ? name + "&" : name;
}

std::string parameterSignature(const FunctionDecl& function) {
	std::string signature;
	for (const Parameter& parameter: function.parameters) {
		signature += (signature.empty() ? "" : ",") + parameterSignature(parameter);
	}
	return signature;
}

}
