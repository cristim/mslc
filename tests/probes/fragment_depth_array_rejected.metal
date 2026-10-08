// EXPECT: error expected a type, found [
struct O { float d[2] [[depth(any)]]; };
fragment O f() { O o; return o; }
