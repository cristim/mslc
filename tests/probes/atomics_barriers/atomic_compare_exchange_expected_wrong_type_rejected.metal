// EXPECT: error takes &expected of the type the atomic holds
// Apple: "no matching function for call to 'atomic_compare_exchange_weak_explicit'".
kernel void k(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  int e = 1;
  atomic_compare_exchange_weak_explicit(c, &e, 2u, memory_order_relaxed, memory_order_relaxed);
}
