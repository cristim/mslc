// EXPECT: error does not lower mem_flags::mem_threadgroup_imageblock
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  threadgroup_barrier(mem_flags::mem_threadgroup_imageblock);
}
