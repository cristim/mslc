// EXPECT: error has to be a scalar uint
struct S { uint v; };
struct O { S m [[sample_mask]]; };
fragment O f() { O o; return o; }
