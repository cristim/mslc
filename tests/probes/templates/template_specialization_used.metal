// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpFAdd %float
// An explicit specialization is the body of the instance it names: f(x) with a float argument is the
// specialization's x + 1, not the primary template's x (the case #341 refused).
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x) { return x; }
template <> float f<float>(float x) { return x + 1.0f; }
kernel void k(device const float* in [[buffer(0)]], device float* out [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = f(in[i]); }
