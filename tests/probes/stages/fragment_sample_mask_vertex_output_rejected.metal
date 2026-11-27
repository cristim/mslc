// EXPECT: error [[sample_mask]], which is not valid on a vertex output
struct O { float4 p [[position]]; uint m [[sample_mask]]; };
vertex O v() { O o; return o; }
