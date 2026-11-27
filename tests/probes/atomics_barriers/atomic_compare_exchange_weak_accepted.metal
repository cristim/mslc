// EXPECT: valid
// DISASM: OpAtomicCompareExchange
// DISASM-NOT: OpAtomicCompareExchangeWeak
// Apple accepts atomic_compare_exchange_weak_explicit on a device atomic buffer element and on a
// threadgroup atomic, as a statement or as a bool; the old value goes back into &expected.
kernel void atomic_compare_exchange_weak_accepted(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]],
    device atomic_int *s [[buffer(2)]]) {
  threadgroup atomic_uint t;
  uint e = 1u;
  bool ok = atomic_compare_exchange_weak_explicit(c, &e, 2u, memory_order_relaxed, memory_order_relaxed);
  int f = 3;
  atomic_compare_exchange_weak_explicit(&s[1], &f, 4, memory_order_relaxed, memory_order_relaxed);
  uint g = 0u;
  if (!atomic_compare_exchange_weak_explicit(&t, &g, 5u, metal::memory_order_relaxed, memory_order_relaxed)) {
    g = 0u;
  }
  o[0] = ok ? e : g + uint(f);
}
