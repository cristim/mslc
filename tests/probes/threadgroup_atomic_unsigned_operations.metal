// EXPECT: valid
// DISASM-MATCH: OpAtomicIAdd %uint %[A-Za-z0-9_]+ %uint_2[A-Za-z0-9_]* %uint_0[A-Za-z0-9_]* %
// DISASM-MATCH: OpAtomicStore %[A-Za-z0-9_]+ %uint_2[A-Za-z0-9_]* %uint_0[A-Za-z0-9_]* %
// DISASM-MATCH: OpAtomicLoad %uint %[A-Za-z0-9_]+ %uint_2[A-Za-z0-9_]* %uint_0[A-Za-z0-9_]*
// DISASM: OpAtomicISub
// DISASM: OpAtomicUMin
// DISASM: OpAtomicUMax
// DISASM: OpAtomicAnd
// DISASM: OpAtomicOr
// DISASM: OpAtomicXor
// DISASM: OpAtomicExchange
// DISASM: OpVariable %_ptr_Workgroup_uint Workgroup
// DISASM-NOT: OpVariable %_ptr_Function_uint Function
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  atomic_store_explicit(&a, 10u, memory_order_relaxed);
  o[0] = atomic_fetch_add_explicit(&a, 5u, memory_order_relaxed);
  o[1] = atomic_fetch_sub_explicit(&a, 3u, memory_order_relaxed);
  o[2] = atomic_fetch_min_explicit(&a, 7u, memory_order_relaxed);
  o[3] = atomic_fetch_max_explicit(&a, 9u, memory_order_relaxed);
  o[4] = atomic_fetch_and_explicit(&a, 12u, memory_order_relaxed);
  o[5] = atomic_fetch_or_explicit(&a, 3u, memory_order_relaxed);
  o[6] = atomic_fetch_xor_explicit(&a, 5u, memory_order_relaxed);
  o[7] = atomic_exchange_explicit(&a, 100u, memory_order_relaxed);
  o[8] = atomic_load_explicit(&a, memory_order_relaxed);
}
