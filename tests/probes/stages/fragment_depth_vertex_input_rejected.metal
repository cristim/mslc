// EXPECT: error only valid on a fragment output
struct I { float d [[depth(any)]]; };
struct O { float4 p [[position]]; };
vertex O f(I i [[stage_in]]) { O o; o.p = float4(i.d); return o; }
