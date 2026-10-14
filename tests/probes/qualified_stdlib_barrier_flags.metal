// EXPECT: valid
// DISASM-MATCH: OpControlBarrier %uint_2 %uint_1 %uint_328
#include <metal_stdlib>
kernel void k(device int *o [[buffer(0)]]){metal::threadgroup_barrier(metal::mem_flags::mem_device | metal::mem_flags::mem_threadgroup);o[0]=o[1]+7;}
