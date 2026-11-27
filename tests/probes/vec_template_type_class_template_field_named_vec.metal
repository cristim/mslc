// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A class template's field named vec, used in a member function, is the field.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct S { T vec; T g() const { return vec < T(3) ? T(1) : T(0); } };
kernel void k(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { S<int> s; s.vec = int(i); out[i] = s.g(); }
