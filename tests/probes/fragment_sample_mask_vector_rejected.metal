// EXPECT: error has to be a scalar uint
struct O { uint2 m [[sample_mask]]; };
fragment O f() { O o; return o; }
