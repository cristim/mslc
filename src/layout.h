#pragma once

#include "ast.h"

#include <cstdint>
#include <vector>

namespace mslc {

// How Metal lays types out in memory, shared by the buffer layout in sema and the sizeof
// the parser folds, so the two cannot disagree.

// Where a member sits in a Metal struct, and how far an array of the struct
// steps. Metal puts a float3 at the next multiple of 16 even though it holds
// 12, and steps 16 too, so a float after it starts at 16 rather than 12.
// indium's own lighting test relies on that: the host side declares
// normalMatrix as "float normalMatrix[3][4]", a 176-byte struct whose matrix
// has 16-byte columns.
struct VectorLayout {
	uint32_t size;
	uint32_t alignment;
};

uint32_t storageBytesOf(ScalarKind kind);
VectorLayout vectorLayoutFor(uint32_t scalarBytes, uint32_t components);
VectorLayout packedVectorLayoutFor(uint32_t scalarBytes, uint32_t components);
VectorLayout matrixLayoutFor(uint32_t scalarBytes, uint32_t columns, uint32_t rows);
uint32_t alignTo(uint32_t offset, uint32_t alignment);
// A struct-typed member is laid out whole when the unit that declares it is given, and has no layout
// of its own here without it.
VectorLayout elementLayout(const Type& type, const TranslationUnit* unit = nullptr);
std::vector<uint32_t> arrayStrides(const Type& type, const TranslationUnit* unit = nullptr);
VectorLayout memberLayout(const Type& type, const TranslationUnit* unit = nullptr);

// A struct's size, rounded up to its alignment, and that alignment: how it sits as a member of another
// struct or as an array element.
VectorLayout structElementLayout(const std::string& name, const TranslationUnit& unit);

// What sizeof gives a type: its size with Metal's alignment and tail padding, a struct as
// big as its members laid out in order rounded up to its own alignment, a pointer 8 bytes.
// Throws a CompileError for a type with no size here (a texture or sampler, an empty or
// undeclared struct, void), so a refusal stands in for a guess.
uint64_t sizeofType(const Type& type, const TranslationUnit& unit);

}
