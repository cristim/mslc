// EXPECT: valid
// Refs #341: a local array reaches a pointer parameter of a member template, beside an argument that is evaluated for deduction.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> float sum(thread U* p, U bias) const { return a + float(p[0]) + float(p[1]) + float(bias); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s{1.0f}; float fa[2] = {2.0f, 3.0f}; int ia[2] = {4, 5}; out[i] = s.sum(fa, 1.5f) + s.sum(ia, 2); }
