// EXPECT: error [[sample_mask]] on a fragment input
struct I { uint m [[sample_mask]]; };
fragment float4 f(I i [[stage_in]]) { return float4(0.75); }
