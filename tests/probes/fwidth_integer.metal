// EXPECT: error fwidth takes float scalars or vectors, half is not supported yet
fragment float4 main() { fwidth(1); return float4(1.0f); }
