// EXPECT: error combines sample_mask with an attribute index, which mslc does not lower
struct O { uint m [[attribute(0), sample_mask]]; };
fragment O f() { O o; return o; }
