// EXPECT: valid
// RUN: grid=16 local=8 cases=4 elems=256
// The operand of a return in a function returning void may be a comma whose right operand is a void call, or a
// conditional whose two values are void calls. Thread 1 adds 1 and 2 to 100; odd threads add 10, even ones 20.
// On an RTX 3060 threads 0 to 3 write 120, 103, 120 and 110.
#include <metal_stdlib>
using namespace metal;
inline void w(device float* o, uint i, float v) { o[i] += v; }
inline void v(device float* o, uint i) {
  
  if (i == 1) { return w(o, i, 1.0f), w(o, i, 2.0f); }
  return (i & 1u) ? w(o, i, 10.0f) : w(o, i, 20.0f);
}
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = 100.0f; v(out, i); }
