// EXPECT: error [[sample_mask]] takes no argument
struct O { uint m [[sample_mask(1)]]; };
fragment O f() { O o; return o; }
