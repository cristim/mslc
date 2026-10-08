// EXPECT: error [[sample_mask]] takes no argument
struct O { uint m [[sample_mask()]]; };
fragment O f() { O o; return o; }
