// EXPECT: error fwidth takes numeric scalars or vectors
fragment float4 main() { fwidth(true); return float4(1.0f); }
