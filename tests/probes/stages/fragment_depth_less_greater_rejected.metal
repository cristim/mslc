// EXPECT: error more than one [[depth]] attribute
struct O { float d [[depth(less), depth(greater)]]; };
fragment O f() { O o; return o; }
