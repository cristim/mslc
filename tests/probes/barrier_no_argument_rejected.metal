// EXPECT: error takes one argument
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  threadgroup_barrier();
}
