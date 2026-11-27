// EXPECT: error incompatible depth attribute combination
struct O { float d [[depth(any), position]]; };
fragment O f() { O o; return o; }
