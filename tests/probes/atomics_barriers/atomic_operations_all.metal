// EXPECT: valid
// DISASM: OpAtomicISub
// DISASM: OpAtomicUMin
// DISASM: OpAtomicUMax
// DISASM: OpAtomicSMin
// DISASM: OpAtomicSMax
// DISASM: OpAtomicAnd
// DISASM: OpAtomicOr
// DISASM: OpAtomicXor
// DISASM: OpAtomicExchange
// DISASM: OpAtomicLoad
// DISASM: OpAtomicStore
// Each operation returns the old value and leaves the new one (lavapipe readback of a one-invocation run).
kernel void atomic_operations_all(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]], device atomic_int *ci [[buffer(2)]]) {
  atomic_store_explicit(c, 10u, memory_order_relaxed);
  o[0] = atomic_fetch_add_explicit(c, 5u, memory_order_relaxed);
  o[1] = atomic_fetch_sub_explicit(c, 3u, memory_order_relaxed);
  o[2] = atomic_fetch_min_explicit(c, 7u, memory_order_relaxed);
  o[3] = atomic_fetch_max_explicit(c, 9u, memory_order_relaxed);
  o[4] = atomic_fetch_and_explicit(c, 12u, memory_order_relaxed);
  o[5] = atomic_fetch_or_explicit(c, 3u, memory_order_relaxed);
  o[6] = atomic_fetch_xor_explicit(c, 5u, memory_order_relaxed);
  o[7] = atomic_exchange_explicit(c, 100u, memory_order_relaxed);
  o[8] = atomic_load_explicit(c, memory_order_relaxed);
  atomic_store_explicit(ci, -5, memory_order_relaxed);
  o[9] = uint(atomic_fetch_min_explicit(ci, -9, memory_order_relaxed));
  o[10] = uint(atomic_fetch_max_explicit(ci, 4, memory_order_relaxed));
  o[11] = uint(atomic_load_explicit(ci, memory_order_relaxed));
  o[12] = atomic_fetch_add_explicit(&c[1], 2u, memory_order_relaxed);
  o[13] = atomic_load_explicit(&c[1], memory_order_relaxed);
}
