// EXPECT: error has to be a scalar uint
struct O { float2x2 m [[sample_mask]]; };
fragment O f() { O o; return o; }
