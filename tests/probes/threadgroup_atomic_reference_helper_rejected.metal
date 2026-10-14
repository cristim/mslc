// EXPECT: error cannot be used as an ordinary place
void take(thread uint &v) { v = 1u; }
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup atomic_uint a;
  take(a);
}
