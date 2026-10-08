// EXPECT: error fwidth takes 1 argument, and this call passes 2
fragment float4 main() { fwidth(1.0f, 2.0f); return float4(1.0f); }
