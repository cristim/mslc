// EXPECT: error a kernel returns void
struct O { float d [[depth(any)]]; };
kernel O f() { O o; return o; }
