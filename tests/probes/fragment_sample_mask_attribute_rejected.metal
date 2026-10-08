// EXPECT: error incompatible sample_mask attribute combination
struct O { uint m [[attribute(0), sample_mask]]; };
fragment O f() { O o; return o; }
