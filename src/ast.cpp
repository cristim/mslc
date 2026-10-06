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

std::string typeName(const Type& type) {
	if (type.resource == ResourceKind::Sampler) {
		return "sampler";
	}
	if (type.resource == ResourceKind::Texture2D) {
		return std::string("texture2d<") + scalarKindName(type.scalar) + ">";
	}
	if (type.resource == ResourceKind::TextureCube) {
		return std::string("texturecube<") + scalarKindName(type.scalar) + ">";
	}

	std::string name = type.namedType.empty()
		? std::string(scalarKindName(type.scalar))
		: type.namedType;

	if (type.namedType.empty() && type.isMatrix()) {
		name += std::to_string(type.matrixColumns) + "x" + std::to_string(type.vectorWidth);
	} else if (type.namedType.empty() && type.vectorWidth > 1) {
		name += std::to_string(type.vectorWidth);
		if (type.isPacked) {
			name = "packed_" + name;
		}
	}
	if (type.arrayLength) {
		name += "[" + std::to_string(*type.arrayLength) + "]";
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

}
