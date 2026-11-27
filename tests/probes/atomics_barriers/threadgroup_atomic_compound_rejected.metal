// EXPECT: error cannot be used as an ordinary place
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  a += 1u;
}
