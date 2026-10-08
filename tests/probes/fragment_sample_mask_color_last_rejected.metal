// EXPECT: error incompatible sample_mask attribute combination
struct O { uint m [[sample_mask, color(0)]]; };
fragment O f() { O o; return o; }
