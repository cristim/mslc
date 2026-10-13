// EXPECT: valid
// mem_none is an execution barrier with no memory semantics.
// DISASM-MATCH: OpControlBarrier %uint_2 %uint_2[^ ]* %uint_0[^ ]*
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  threadgroup_barrier(mem_flags::mem_none);
  o[t] = 1.0;
}
