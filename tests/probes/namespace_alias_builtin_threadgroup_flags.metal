// EXPECT: valid
// DISASM-MATCH: OpControlBarrier %uint_2 %uint_2[^ ]* %uint_264
#include <metal_stdlib>
namespace N { namespace M = ::metal; }
kernel void k(device float* out [[buffer(0)]]) {
    N::M::threadgroup_barrier(N::M::mem_flags::mem_threadgroup); out[0] = 11.0f;
}
