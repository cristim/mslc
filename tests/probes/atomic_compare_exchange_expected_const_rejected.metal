// EXPECT: error which is const
// Apple: "no matching function for call to 'atomic_compare_exchange_weak_explicit'".
kernel void k(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  const uint e = 1u;
  atomic_compare_exchange_weak_explicit(c, &e, 2u, memory_order_relaxed, memory_order_relaxed);
}
