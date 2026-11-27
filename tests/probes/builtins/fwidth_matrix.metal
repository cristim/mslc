// EXPECT: error fwidth takes scalars or vectors, not a matrix
fragment float4 main() { fwidth(float2x2(float2(1.0f), float2(2.0f))); return float4(1.0f); }
