// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpIMul %int
// A specialization of a template that takes const T& and returns T.
#include <metal_stdlib>
using namespace metal;
template <typename T> T twice(thread const T& x) { return x + x; }
template <> int twice(thread const int& x) { return x * 2; }
kernel void k(device const int* in [[buffer(0)]], device int* out [[buffer(1)]], uint i [[thread_position_in_grid]])
{ const int v = in[i]; out[i] = twice(v) + int(twice(float(v))); }
