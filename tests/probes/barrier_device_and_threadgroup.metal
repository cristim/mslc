// EXPECT: valid
// A device flag widens the memory scope to Device and adds UniformMemory (8 | 64 | 256 = 328).
// DISASM-MATCH: OpControlBarrier %uint_2 %uint_1 %uint_328
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  threadgroup_barrier(metal::mem_flags::mem_device | mem_flags::mem_threadgroup);
  o[t] = 1.0;
}
