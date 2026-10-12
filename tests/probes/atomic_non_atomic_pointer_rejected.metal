// EXPECT: error is not one
kernel void atomic_non_atomic_pointer_rejected(device uint *o [[buffer(0)]]) {
  o[0] = atomic_fetch_add_explicit(o, 1, memory_order_relaxed);
}
