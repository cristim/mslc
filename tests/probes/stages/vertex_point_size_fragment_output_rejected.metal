// EXPECT: error [[point_size]], which is only valid on a vertex output
struct O { float s [[point_size]]; };
fragment O f() { O o; o.s = 1.0; return o; }
