// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: %float_2
// DISASM: %float_3
// DISASM: %float_4
// The specialization's arguments are deduced from its parameter types ("template <> float f(short)") or
// written ("f<int>"); a call reaches it by deduction or with explicit arguments, and an instance with no
// specialization is the primary template's.
#include <metal_stdlib>
using namespace metal;
template <typename T> static inline float f(T x) { return float(x) * 2.0f; }
template <> inline float f(short x) { return float(x) * 3.0f; }
template <> inline float f<int>(int x) { return float(x) * 4.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(i) + f(short(i)) + f<int>(int(i)) + f(int(i)); }
