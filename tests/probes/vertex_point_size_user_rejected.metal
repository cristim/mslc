// EXPECT: error combines point_size with user or interpolation, which mslc does not lower
struct O { float4 p [[position]]; float s [[point_size, user(locn0)]]; };
vertex O f(uint vid [[vertex_id]]) { O o; o.p = float4(0.0); o.s = 1.0; return o; }
