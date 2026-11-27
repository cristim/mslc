// EXPECT: valid
// mem_texture is ImageMemory (2048) at Device scope, with AcquireRelease (8).
// DISASM-MATCH: OpControlBarrier %uint_2 %uint_1 %uint_2056
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  threadgroup_barrier(mem_flags::mem_texture);
  o[t] = 1.0;
}
