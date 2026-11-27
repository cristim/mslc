// EXPECT: error combines sample_mask with user or interpolation
struct O { uint m [[sample_mask, flat]]; };
fragment O f() { O o; return o; }
