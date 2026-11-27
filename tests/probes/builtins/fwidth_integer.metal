// EXPECT: error fwidth takes float or half scalars or vectors
fragment float4 main() { fwidth(1); return float4(1.0f); }
