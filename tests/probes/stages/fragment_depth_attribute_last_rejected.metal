// EXPECT: error combines depth with an attribute index, which mslc does not lower
struct O { float d [[attribute(0), depth(any)]]; };
fragment O f() { O o; return o; }
