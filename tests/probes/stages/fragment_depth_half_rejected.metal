// EXPECT: error has to be a scalar float
struct O { half d [[depth(any)]]; };
fragment O f() { O o; return o; }
