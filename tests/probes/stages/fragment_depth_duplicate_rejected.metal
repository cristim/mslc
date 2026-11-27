// EXPECT: error more than one [[depth]] attribute
struct O { float d [[depth(any), depth(any)]]; };
fragment O f() { O o; return o; }
