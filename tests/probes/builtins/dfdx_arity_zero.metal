// EXPECT: error dfdx takes 1 argument, and this call passes 0
fragment float4 main() { dfdx(); return float4(1.0f); }
