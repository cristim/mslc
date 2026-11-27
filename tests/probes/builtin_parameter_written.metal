// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A builtin parameter is an ordinary variable of the entry point that starts as the builtin's value, so the body
// may assign it, step it and bind a reference to it ([dcl.fct]). With one threadgroup of 64, thread i writes
// 5 + 100 * (i + 1) + 10000 to out[i].
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint2 g [[thread_position_in_grid]], ushort l [[thread_index_in_threadgroup]], uint i [[threadgroup_position_in_grid]]) {
  g.y += 5; l++; thread uint& r = i; r *= 2; ++i;
  out[g.x] = float(g.y) + float(l) * 100.0f + float(i) * 10000.0f;
}
