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
	std::string name = type.namedType.empty()
		? std::string(scalarKindName(type.scalar))
		: type.namedType;

	if (type.namedType.empty() && type.vectorWidth > 1) {
		name += std::to_string(type.vectorWidth);
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
