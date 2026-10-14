// EXPECT: error cannot be read as an ordinary value
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  o[0] = a;
}
