// EXPECT: valid
// The corpus use: a fragment function allocates a slot with a device atomic counter.
fragment float4 atomic_in_fragment_function(device atomic_uint *listIndex [[buffer(1)]]) {
  uint slot = atomic_fetch_add_explicit(listIndex, 1, memory_order_relaxed);
  return float4(float(slot));
}
