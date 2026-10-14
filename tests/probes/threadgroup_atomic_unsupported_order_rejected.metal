// EXPECT: error takes memory_order_relaxed
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  o[0] = atomic_load_explicit(&a, memory_order_seq_cst);
}
