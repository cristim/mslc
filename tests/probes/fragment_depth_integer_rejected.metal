// EXPECT: error attribute "depth" needs an identifier
struct O { float d [[depth(0)]]; };
fragment O f() { O o; return o; }
