// EXPECT: valid
// simdgroup_barrier is a Subgroup execution barrier at Subgroup memory scope.
// DISASM-MATCH: OpControlBarrier %uint_3 %uint_3[^ ]* %uint_264
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  simdgroup_barrier(mem_flags::mem_threadgroup);
  o[t] = 1.0;
}
