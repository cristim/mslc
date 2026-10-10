// EXPECT: error a kernel returns void
struct O { float s [[point_size]]; };
kernel O f() { O o; o.s = 1.0; return o; }
