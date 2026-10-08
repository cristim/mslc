// EXPECT: error incompatible depth attribute combination
struct O { uint m [[sample_mask, depth(any)]]; };
fragment O f() { O o; return o; }
