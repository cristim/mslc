// EXPECT: error is not lowered yet
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  uint expected = 0; atomic_compare_exchange_weak_explicit(&a, &expected, 1u, memory_order_relaxed, memory_order_relaxed);
}
