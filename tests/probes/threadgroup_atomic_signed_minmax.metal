// EXPECT: valid
// DISASM-MATCH: OpAtomicSMin %int %[A-Za-z0-9_]+ %uint_2[A-Za-z0-9_]* %uint_0[A-Za-z0-9_]* %
// DISASM-MATCH: OpAtomicSMax %int %[A-Za-z0-9_]+ %uint_2[A-Za-z0-9_]* %uint_0[A-Za-z0-9_]* %
kernel void k(device int *o [[buffer(0)]]) {
  threadgroup atomic_int a;
  atomic_store_explicit(&a, 5, memory_order_relaxed);
  o[0] = atomic_fetch_min_explicit(&a, -9, memory_order_relaxed);
  o[1] = atomic_load_explicit(&a, memory_order_relaxed);
  o[2] = atomic_fetch_max_explicit(&a, 4, memory_order_relaxed);
  o[3] = atomic_load_explicit(&a, memory_order_relaxed);
}
