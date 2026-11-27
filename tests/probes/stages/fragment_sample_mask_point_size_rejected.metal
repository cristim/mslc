// EXPECT: error incompatible sample_mask attribute combination
struct O { uint m [[sample_mask, point_size]]; float4 c [[color(0)]]; };
fragment O f() { O o; return o; }
