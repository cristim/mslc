// EXPECT: error takes &expected, the address of a local scalar variable
// Apple: "no matching function": expected is a thread pointer, not a device one.
kernel void k(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  atomic_compare_exchange_weak_explicit(c, &o[0], 2u, memory_order_relaxed, memory_order_relaxed);
}
