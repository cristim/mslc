// EXPECT: valid
// DISASM: OpControlBarrier %uint_2 %uint_2_0 %uint_264
// DISASM: OpControlBarrier %uint_3 %uint_3_0 %uint_0_0
// M::mem_flags::mem_threadgroup is the workgroup-memory flag (semantics 264 = 0x108) and
// M::simdgroup_barrier is the subgroup barrier (scope 3); mem_none is no semantics.
#include <metal_stdlib>
namespace M = metal;
kernel void kern(device float* out [[buffer(0)]]) { M::threadgroup_barrier(M::mem_flags::mem_threadgroup); M::simdgroup_barrier(M::mem_flags::mem_none); out[0] = 1; }
