// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpTypeStruct %int
// A return type Box<T> whose T only an explicit argument gives.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> Box<T> make(float x) { Box<T> b; b.v = T(x); return b; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { Box<int> b = make<int>(float(i) + 0.5f); out[i] = float(b.v); }
