// EXPECT: error more than one [[depth]] field
struct O { float d [[depth(any)]]; float e [[depth(less)]]; };
fragment O f() { O o; return o; }
