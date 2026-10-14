// EXPECT: error with an initialiser
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a = 1u;

}
