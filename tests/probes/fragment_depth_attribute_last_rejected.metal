// EXPECT: error incompatible depth attribute combination
struct O { float d [[attribute(0), depth(any)]]; };
fragment O f() { O o; return o; }
