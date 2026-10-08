// EXPECT: error incompatible depth attribute combination
struct O { uint m [[depth(any), sample_mask]]; };
fragment O f() { O o; return o; }
