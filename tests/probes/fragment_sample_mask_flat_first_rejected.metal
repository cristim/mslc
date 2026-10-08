// EXPECT: error combines sample_mask with user or interpolation
struct O { uint m [[flat, sample_mask]]; };
fragment O f() { O o; return o; }
