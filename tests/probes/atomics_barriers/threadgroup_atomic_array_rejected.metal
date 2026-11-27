// EXPECT: error a local atomic
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a[2];

}
