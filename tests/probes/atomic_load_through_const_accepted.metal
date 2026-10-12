// EXPECT: valid
// A const atomic can be loaded (Apple accepts it) but not changed.
kernel void atomic_load_through_const_accepted(device uint *o [[buffer(0)]], device const atomic_uint *c [[buffer(1)]]) {
  o[0] = atomic_load_explicit(c, memory_order_relaxed);
}
