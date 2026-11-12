// EXPECT: valid
// DISASM-MATCH: OpControlBarrier %uint_3 %uint_3[^ ]* %uint_264
#include <metal_stdlib>
namespace M = ::metal;
kernel void k(device float* out [[buffer(0)]]) {
    M::simdgroup_barrier(M::mem_flags::mem_threadgroup); out[0] = 11.0f;
}
