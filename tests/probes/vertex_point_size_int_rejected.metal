// EXPECT: error has to be a scalar float
struct O { float4 p [[position]]; int s [[point_size]]; };
vertex O f(uint vid [[vertex_id]]) { O o; o.p = float4(0.0); o.s = 1; return o; }
