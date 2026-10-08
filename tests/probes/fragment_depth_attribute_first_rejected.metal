// EXPECT: error incompatible depth attribute combination
struct O { float d [[depth(any), attribute(0)]]; };
fragment O f() { O o; return o; }
