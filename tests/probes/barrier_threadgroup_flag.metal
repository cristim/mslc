// EXPECT: valid
// threadgroup_barrier(mem_threadgroup) is an execution barrier that orders workgroup memory.
// DISASM-MATCH: OpControlBarrier %uint_2 %uint_2[^ ]* %uint_264
// DISASM-NOT: OpMemoryBarrier
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  threadgroup_barrier(mem_flags::mem_threadgroup);
  o[t] = 1.0;
}
