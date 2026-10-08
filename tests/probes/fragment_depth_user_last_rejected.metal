// EXPECT: error combines depth with user or interpolation, which mslc does not lower
struct O { float d [[user(locn0), depth(any)]]; };
fragment O f() { O o; return o; }
