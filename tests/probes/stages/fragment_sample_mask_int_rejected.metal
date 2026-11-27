// EXPECT: error has to be a scalar uint
struct O { int m [[sample_mask]]; };
fragment O f() { O o; return o; }
