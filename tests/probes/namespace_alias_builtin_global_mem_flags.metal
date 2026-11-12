// EXPECT: valid
// DISASM-MATCH: OpControlBarrier %uint_2 %uint_2[^ ]* %uint_264
#include <metal_stdlib>
enum mem_flags { mem_threadgroup = 7 };
namespace M = ::metal;
kernel void k(device int* out [[buffer(0)]]) {
    M::threadgroup_barrier(M::mem_flags::mem_threadgroup);
    out[0] = 11;
}
