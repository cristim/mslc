// EXPECT: error has to be a scalar uint
struct O { device uint* m [[sample_mask]]; };
fragment O f() { O o; return o; }
