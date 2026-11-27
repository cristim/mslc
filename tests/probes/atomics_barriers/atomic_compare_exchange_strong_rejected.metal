// EXPECT: error is not a Metal function
// Apple: "use of undeclared identifier 'atomic_compare_exchange_strong_explicit'; did you mean
// 'atomic_compare_exchange_weak_explicit'?" (metal 32023.864): only the weak form exists.
kernel void k(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  uint e = 1u;
  atomic_compare_exchange_strong_explicit(c, &e, 2u, memory_order_relaxed, memory_order_relaxed);
}
