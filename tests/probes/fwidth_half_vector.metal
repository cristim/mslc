// EXPECT: error fwidth takes float scalars or vectors, half is not supported yet
fragment float4 main() { fwidth(half2(1.0f)); return float4(1.0f); }
