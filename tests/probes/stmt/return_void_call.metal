// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A function returning void may return a call to another function returning void ([stmt.return]), a helper
// as an entry point: thread i writes i + 0.25 for i = 1 and i + 0.5 otherwise.
#include <metal_stdlib>
using namespace metal;
inline void w(device float* o, uint i, float v) { o[i] = float(i) + v; }
inline void v(device float* o, uint i) { if (i == 1) { return w(o, i, 0.25f); } return w(o, i, 0.5f); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { return v(out, i); }
