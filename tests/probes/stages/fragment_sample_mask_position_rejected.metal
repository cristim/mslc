// EXPECT: error incompatible sample_mask attribute combination
struct O { uint m [[sample_mask, position]]; };
fragment O f() { O o; return o; }
