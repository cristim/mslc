// EXPECT: error more than one [[depth]] attribute
struct O { float d [[depth(greater), depth(less)]]; };
fragment O f() { O o; return o; }
