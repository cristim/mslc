// EXPECT: valid
// DISASM-MATCH: OpControlBarrier
// DISASM-MATCH: OpImageWrite
// A builtin returning void, a barrier or a texture write, may be the operand of a return in a function returning
// void, as a call to a helper returning void may.
#include <metal_stdlib>
using namespace metal;
inline void sync() { return threadgroup_barrier(mem_flags::mem_threadgroup); }
kernel void k(device float* out [[buffer(0)]], texture2d<float, access::write> t [[texture(0)]],
              uint2 g [[thread_position_in_grid]]) {
  sync();
  out[g.x] = 1.0f;
  return t.write(float4(1.0f), g);
}
