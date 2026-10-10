// EXPECT: error has to be a scalar float
struct O { float4 p [[position]]; float[2] s [[point_size]]; };
vertex O f(uint vid [[vertex_id]]) { O o; o.p = float4(0.0); o.s[0] = 1.0; o.s[1] = 1.0; return o; }
