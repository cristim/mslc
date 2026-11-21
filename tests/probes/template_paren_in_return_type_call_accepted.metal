// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-MATCH: OpFunctionCall %float
// Refs #341: the name is the one before the parameter list, not the one before the "(" inside the
// return type's angle brackets. The return type is an enable_if over sizeof(T): the float (4 bytes)
// keeps g, and the half (2 bytes) drops it, so the helper takes that call.
#include <metal_stdlib>
using namespace metal;
template <typename T> typename enable_if<(sizeof(T) > 2), T>::type g(T x) { return x; }
float g(half x) { return 7.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = g(1.0f) + g(half(1.0h)); }
