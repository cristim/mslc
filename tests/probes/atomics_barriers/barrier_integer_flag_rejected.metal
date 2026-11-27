// EXPECT: error takes mem_flags::mem_none, mem_device, mem_threadgroup or mem_texture
kernel void k(device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  threadgroup_barrier(0);
}
