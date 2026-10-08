// EXPECT: error incompatible depth attribute combination
struct O { float d [[depth(any), color(0)]]; };
fragment O f() { O o; return o; }
