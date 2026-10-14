// EXPECT: error can only be declared in a kernel function
fragment float4 k() {
  threadgroup atomic_uint a;
  return float4(0.0);
}
