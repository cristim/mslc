// EXPECT: valid
// DISASM-MATCH: OpAtomicCompareExchange %uint %[0-9]+ %uint_2[_0-9]* %uint_0[_0-9]* %uint_0[_0-9]* %uint_9 %[0-9]+
// DISASM-NO-MATCH: OpAtomicCompareExchange %uint %[0-9]+ %uint_1[_0-9]* 
// A threadgroup atomic is exchanged at Workgroup scope (2), not Device (1).
kernel void atomic_compare_exchange_weak_threadgroup_scope(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  uint e = 5u;
  o[0] = atomic_compare_exchange_weak_explicit(&a, &e, 9u, memory_order_relaxed, memory_order_relaxed) ? 1u : 0u;
}
