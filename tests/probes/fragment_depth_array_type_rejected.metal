// EXPECT: error has to be a scalar float
struct O { float[2] d [[depth(any)]]; };
fragment O f() { O o; return o; }
