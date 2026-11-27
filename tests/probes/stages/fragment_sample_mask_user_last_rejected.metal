// EXPECT: error combines sample_mask with user or interpolation
struct O { uint m [[sample_mask, user(mask)]]; };
fragment O f() { O o; return o; }
