// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A struct field named vec is the field, not the library's vec<T, N>: "vec < 3" in a member function compares it.
#include <metal_stdlib>
using namespace metal;
struct S { int vec; int g() const { return vec < 3 ? 1 : 0; } };
kernel void k(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { S s; s.vec = int(i); out[i] = s.g(); }
