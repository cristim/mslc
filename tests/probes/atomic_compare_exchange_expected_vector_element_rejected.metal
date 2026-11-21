// EXPECT: error takes &expected, the address of a local scalar variable
// Apple: "address of vector element requested".
kernel void k(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  uint2 v = uint2(1u);
  atomic_compare_exchange_weak_explicit(c, &v.x, 2u, memory_order_relaxed, memory_order_relaxed);
}
