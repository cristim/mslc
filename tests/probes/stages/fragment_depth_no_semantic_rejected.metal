// EXPECT: error has no [[color(n)]]
struct O { float d [[depth(any)]]; float e; };
fragment O f() { O o; return o; }
