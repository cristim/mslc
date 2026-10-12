// EXPECT: valid
// DISASM-MATCH: OpAtomicIAdd %uint %[A-Za-z0-9_]+ %uint_1[A-Za-z0-9_]* %uint_0[A-Za-z0-9_]* %
// A device atomic_uint counter: the returned old value is a unique slot for each invocation (lavapipe, 16
// invocations: all 16 slots written once and the counter ends at 16; fetch_sub in its place fails that check).
kernel void atomic_fetch_add_counter(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  uint old = atomic_fetch_add_explicit(c, 1, memory_order_relaxed);
  o[old] = 1u;
}
