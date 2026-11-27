// EXPECT: error combines depth with user or interpolation, which mslc does not lower
struct O { float d [[depth(any), user(locn0)]]; };
fragment O f() { O o; return o; }
