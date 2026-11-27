// EXPECT: error has to be a scalar float
struct O { float4 p [[position]]; float2 s [[point_size]]; };
vertex O f(uint vid [[vertex_id]]) { O o; o.p = float4(0.0); o.s = float2(1.0); return o; }
