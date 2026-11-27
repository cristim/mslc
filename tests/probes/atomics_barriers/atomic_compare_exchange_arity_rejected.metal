// EXPECT: error takes 5 arguments
// Apple: "no matching function for call to 'atomic_compare_exchange_weak_explicit'".
kernel void k(device uint *o [[buffer(0)]], device atomic_int *c [[buffer(1)]]) {
  int e = 1;
  atomic_compare_exchange_weak_explicit(c, &e, 2, memory_order_relaxed);
}
