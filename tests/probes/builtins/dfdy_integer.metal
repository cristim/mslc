// EXPECT: error dfdy takes float or half scalars or vectors
fragment float4 main() { int i = dfdy(1); return float4(i); }
