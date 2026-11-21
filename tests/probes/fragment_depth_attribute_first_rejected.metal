// EXPECT: error combines depth with an attribute index, which mslc does not lower
struct O { float d [[depth(any), attribute(0)]]; };
fragment O f() { O o; return o; }
