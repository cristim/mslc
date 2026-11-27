// EXPECT: error a local atomic
kernel void atomic_local_rejected(device uint *o [[buffer(0)]]) {
  atomic_uint a;
  o[0] = 1;
}
