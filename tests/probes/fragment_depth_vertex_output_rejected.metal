// EXPECT: error only valid on a fragment output
struct O { float d [[depth(any)]]; };
vertex O f() { O o; return o; }
