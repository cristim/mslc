// EXPECT: error returns void and has no value
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  int x = threadgroup_barrier(mem_flags::mem_none);
}
