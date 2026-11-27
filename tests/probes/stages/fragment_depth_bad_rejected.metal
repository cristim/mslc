// EXPECT: error [[depth]] needs one of any, less or greater
struct O { float d [[depth(sideways)]]; };
fragment O f() { O o; return o; }
