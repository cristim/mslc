// EXPECT: error more than one [[sample_mask]] attribute
struct O { uint m [[sample_mask, sample_mask]]; };
fragment O f() { O o; return o; }
