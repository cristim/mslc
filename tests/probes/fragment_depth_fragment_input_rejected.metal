// EXPECT: error only valid on a fragment output
struct I { float d [[depth(any)]]; };
fragment float4 f(I i [[stage_in]]) { return float4(i.d); }
