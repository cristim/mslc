// EXPECT: error has to be a scalar uint
struct O { uint m[1] [[sample_mask]]; };
fragment O f() { O o; return o; }
