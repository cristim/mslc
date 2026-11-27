#include "layout.h"

#include "lexer.h"

#include <algorithm>

namespace mslc {

// How many bytes a scalar takes in a buffer. A bool is one byte there, though
// SPIR-V gives OpTypeBool no size of its own.
uint32_t storageBytesOf(ScalarKind kind) {
	return kind == ScalarKind::Bool ? 1u : scalarBitWidth(kind) / 8;
}

VectorLayout vectorLayoutFor(uint32_t scalarBytes, uint32_t components) {
	return { (components == 3 ? 4u : components) * scalarBytes,
		(components >= 3 ? 4u : components) * scalarBytes };
}

// A packed vector is its components back to back, aligned as a component is: a
// packed_float3 is 12 bytes at any multiple of 4, where a float3 is 16 at a
// multiple of 16.
VectorLayout packedVectorLayoutFor(uint32_t scalarBytes, uint32_t components) {
	return { components * scalarBytes, scalarBytes };
}

// A matrix is its columns laid out as an array of column vectors, so a
// float3x3 is three 16-byte columns, 48 bytes, and not nine packed floats.
// The column's own size is the MatrixStride.
VectorLayout matrixLayoutFor(uint32_t scalarBytes, uint32_t columns, uint32_t rows) {
	const VectorLayout column = vectorLayoutFor(scalarBytes, rows);
	return { columns * column.size, column.alignment };
}

// The next multiple of the alignment at or after the offset, which is where
// a member of that alignment starts in a struct.
uint32_t alignTo(uint32_t offset, uint32_t alignment) {
	return (offset + alignment - 1) / alignment * alignment;
}

// The element a member is made of, before any array dimension: what Metal
// lays out for one scalar, vector or matrix of its type, or for a pointer. A
// struct element (given the unit that declares it) is the struct laid out whole.
VectorLayout elementLayout(const Type& type, const TranslationUnit* unit) {
	if (type.isPointer) {
		return { 8, 8 };
	}
	if (!type.namedType.empty() && unit) {
		return structElementLayout(type.namedType, *unit);
	}
	const uint32_t components = type.vectorWidth > 1 ? type.vectorWidth : 1;
	const uint32_t scalarBytes = storageBytesOf(type.scalar);
	return type.isMatrix() ? matrixLayoutFor(scalarBytes, type.matrixColumns, components)
		: type.isPacked ? packedVectorLayoutFor(scalarBytes, components)
		: vectorLayoutFor(scalarBytes, components);
}

// The byte step of each array dimension of a member, outermost first. An array
// steps by its element's size, which Metal has already rounded up to the
// element's alignment, so a float3[4] steps 16 and a float[4] steps 4.
std::vector<uint32_t> arrayStrides(const Type& type, const TranslationUnit* unit) {
	std::vector<uint32_t> lengths;
	if (type.arrayLength) {
		lengths.push_back(*type.arrayLength);
		lengths.insert(lengths.end(), type.innerArrayLengths.begin(), type.innerArrayLengths.end());
	}

	const VectorLayout element = elementLayout(type, unit);
	uint64_t stride = alignTo(element.size, element.alignment);
	std::vector<uint32_t> strides(lengths.size());
	for (size_t dimension = lengths.size(); dimension-- > 0;) {
		strides[dimension] = static_cast<uint32_t>(stride);
		stride *= lengths[dimension];
		if (stride > UINT32_MAX) {
			throw CompileError("an array member is larger than 4 GiB, which a buffer offset cannot hold");
		}
	}

	return strides;
}

// A member's size and alignment in a laid-out struct: its element's alignment,
// and the element's step times every dimension for an array.
VectorLayout memberLayout(const Type& type, const TranslationUnit* unit) {
	VectorLayout layout = elementLayout(type, unit);
	if (type.arrayLength) {
		const std::vector<uint32_t> strides = arrayStrides(type, unit);
		const uint64_t size = uint64_t(strides.front()) * *type.arrayLength;
		if (size > UINT32_MAX) {
			throw CompileError("an array member is larger than 4 GiB, which a buffer offset cannot hold");
		}
		layout.size = static_cast<uint32_t>(size);
	}

	return layout;
}

namespace {

// A struct's size and alignment, which a nested member and an array of it both take whole.
struct TypeLayout {
	uint64_t size;
	uint32_t alignment;
};

TypeLayout layoutOf(const Type& type, const TranslationUnit& unit, unsigned depth);

TypeLayout structLayoutOf(const std::string& name, const TranslationUnit& unit, unsigned depth) {
	const StructDecl* decl = unit.findStruct(name);
	if (!decl) {
		throw CompileError("sizeof of \"" + name + "\", which is not a struct this source has declared "
			"in full");
	}
	if (depth > 32) {
		throw CompileError("sizeof of \"" + name + "\" nests structs too deeply");
	}

	uint64_t offset = 0;
	uint32_t alignment = 1;
	for (const StructField& field: decl->fields) {
		const TypeLayout member = layoutOf(field.type, unit, depth + 1);
		offset = (offset + member.alignment - 1) / member.alignment * member.alignment + member.size;
		alignment = std::max(alignment, member.alignment);
	}
	// An empty struct is one byte, so two distinct objects have distinct addresses.
	return { std::max<uint64_t>(1, (offset + alignment - 1) / alignment * alignment), alignment };
}

TypeLayout layoutOf(const Type& type, const TranslationUnit& unit, unsigned depth) {
	TypeLayout element{ 0, 1 };
	if (type.isPointer || type.resource != ResourceKind::None) {
		// A texture or a sampler is a handle of one pointer's size (Apple: 8).
		element = { 8, 8 };
	} else if (!type.namedType.empty()) {
		element = structLayoutOf(type.namedType, unit, depth);
	} else if (type.scalar == ScalarKind::Void) {
		throw CompileError("sizeof of void");
	} else {
		const VectorLayout vector = elementLayout(type);
		element = { vector.size, vector.alignment };
	}

	// An array steps by its element's size, which already ends on its alignment.
	uint64_t size = element.size;
	if (type.arrayLength) {
		std::vector<uint32_t> lengths{ *type.arrayLength };
		lengths.insert(lengths.end(), type.innerArrayLengths.begin(), type.innerArrayLengths.end());
		for (const uint32_t length: lengths) {
			if (length != 0 && size > (uint64_t(1) << 62) / length) {
				throw CompileError("sizeof of an array too large to count");
			}
			size *= length;
		}
	}
	return { size, element.alignment };
}

}

uint64_t sizeofType(const Type& type, const TranslationUnit& unit) {
	return layoutOf(type, unit, 0).size;
}

VectorLayout structElementLayout(const std::string& name, const TranslationUnit& unit) {
	const TypeLayout layout = structLayoutOf(name, unit, 0);
	if (layout.size > UINT32_MAX) {
		throw CompileError("struct \"" + name + "\" is larger than 4 GiB, which a buffer offset cannot hold");
	}
	return { static_cast<uint32_t>(layout.size), layout.alignment };
}

}
