// EXPECT: error [[sample_mask]], which is not valid on a vertex input
struct I { uint m [[sample_mask]]; };
struct V { float4 p [[position]]; };
vertex V v(I i [[stage_in]]) { V o; return o; }
