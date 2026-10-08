// EXPECT: error combines depth with user or interpolation, which mslc does not lower
struct O { float d [[flat, depth(any)]]; };
fragment O f() { O o; return o; }
