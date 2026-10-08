// EXPECT: error fwidth takes 1 argument, and this call passes 0
fragment float4 main() { fwidth(); return float4(1.0f); }
