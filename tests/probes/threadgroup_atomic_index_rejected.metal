// EXPECT: error takes the address of a scalar threadgroup atomic
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  o[0] = atomic_load_explicit(&a[0], memory_order_relaxed);
}
