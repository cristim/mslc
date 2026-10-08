// EXPECT: error incompatible depth attribute combination
struct O { float d [[position, depth(any)]]; };
fragment O f() { O o; return o; }
