// EXPECT: valid
// A barrier inside control flow is lowered where it stands; uniformity is the author's.
// DISASM-MATCH: OpControlBarrier
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  if (t < 4) { threadgroup_barrier(mem_flags::mem_none); }
  o[t] = 1.0;
}
