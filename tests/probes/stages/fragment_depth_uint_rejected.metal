// EXPECT: error has to be a scalar float
struct O { uint d [[depth(any)]]; };
fragment O f() { O o; return o; }
