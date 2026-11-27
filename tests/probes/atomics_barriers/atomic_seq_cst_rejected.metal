// EXPECT: error takes memory_order_relaxed, the only memory order Metal has
// Apple: "no matching function for call to atomic_fetch_add_explicit".
kernel void atomic_seq_cst_rejected(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  o[0] = atomic_fetch_add_explicit(c, 1, memory_order_seq_cst);
}
