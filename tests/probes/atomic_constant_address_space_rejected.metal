// EXPECT: error points into the constant address space
kernel void atomic_constant_address_space_rejected(device uint *o [[buffer(0)]], constant atomic_uint *c [[buffer(1)]]) {
  o[0] = atomic_load_explicit(c, memory_order_relaxed);
}
