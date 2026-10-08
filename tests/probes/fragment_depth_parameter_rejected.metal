// EXPECT: error not valid on an input parameter
fragment float4 f(float d [[depth(any)]]) { return float4(d); }
