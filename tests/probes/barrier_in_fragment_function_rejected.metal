// EXPECT: error only supported in a kernel function
fragment float4 f() {
  threadgroup_barrier(mem_flags::mem_none);
  return float4(1.0);
}
