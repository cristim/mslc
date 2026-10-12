// EXPECT: error is not lowered yet
// Apple accepts the compare-exchange functions; mslc lowers the fetch, exchange, load and store ones.
kernel void atomic_compare_exchange_not_lowered(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  uint e = 1;
  bool ok = atomic_compare_exchange_weak_explicit(c, &e, 2u, memory_order_relaxed, memory_order_relaxed);
  o[0] = ok ? 1u : 0u;
}
