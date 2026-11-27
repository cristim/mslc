// EXPECT: valid
// DISASM: OpAtomicStore
// DISASM: OpAtomicIAdd
// DISASM: OpAtomicLoad
// An alias of atomic_uint keeps the atomic identity through a chain, const either side, a device
// pointer and a threadgroup local; Apple accepts all of it. Repeating the same typedef is a no-op.
typedef atomic_uint Counter;
typedef Counter Counter2;
typedef const atomic_uint ConstCounter;
typedef atomic_uint const ConstCounter2;
typedef atomic_uint Counter;
kernel void k(device uint *out [[buffer(0)]], device Counter2 *c [[buffer(1)]],
    device ConstCounter *cc [[buffer(2)]], device ConstCounter2 *dd [[buffer(3)]]) {
  threadgroup Counter2 a;
  atomic_store_explicit(&a, 7u, memory_order_relaxed);
  atomic_fetch_add_explicit(&c[0], 1u, memory_order_relaxed);
  out[0] = atomic_load_explicit(&a, memory_order_relaxed) + atomic_load_explicit(cc, memory_order_relaxed)
    + atomic_load_explicit(dd, memory_order_relaxed);
}
