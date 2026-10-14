// EXPECT: valid
// DISASM-MATCH: OpAtomicIAdd %uint %[A-Za-z0-9_]+ %uint_2[A-Za-z0-9_]* %uint_0[A-Za-z0-9_]* %
kernel void k(device uint *o [[buffer(0)]], uint t [[thread_position_in_threadgroup]], uint g [[threadgroup_position_in_grid]]) {
  threadgroup atomic_uint a;
  if (t == 0) { atomic_store_explicit(&a, 0u, memory_order_relaxed); }
  threadgroup_barrier(mem_flags::mem_threadgroup);
  o[g * 33 + t] = atomic_fetch_add_explicit(&a, 1u, memory_order_relaxed);
  threadgroup_barrier(mem_flags::mem_threadgroup);
  if (t == 0) { o[g * 33 + 32] = atomic_load_explicit(&a, memory_order_relaxed); }
}
