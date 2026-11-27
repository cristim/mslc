// EXPECT: error cannot be read as an ordinary value
uint take(uint v) { return v; }
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  o[0] = take(a);
}
