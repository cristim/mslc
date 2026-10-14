// EXPECT: valid
// DISASM-MATCH: OpEntryPoint GLCompute %[0-9]+ "k" %[0-9]+ %[0-9]+ %[0-9]+
// DISASM-MATCH: OpAtomicLoad %int %[A-Za-z0-9_]+ %uint_2[A-Za-z0-9_]* %uint_0[A-Za-z0-9_]*
kernel void k(device int *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  atomic_store_explicit(&a, 7u, memory_order_relaxed);
  {
    threadgroup atomic_int b;
    atomic_store_explicit(&b, -3, memory_order_relaxed);
    o[0] = int(atomic_load_explicit(&a, memory_order_relaxed));
    o[1] = atomic_load_explicit(&b, memory_order_relaxed);
  }
}
