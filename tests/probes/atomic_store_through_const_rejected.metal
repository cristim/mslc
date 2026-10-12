// EXPECT: error points at a constant or const one
kernel void atomic_store_through_const_rejected(device const atomic_uint *c [[buffer(1)]]) {
  atomic_store_explicit(c, 5u, memory_order_relaxed);
}
