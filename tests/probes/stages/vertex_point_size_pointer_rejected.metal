// EXPECT: error has to be a scalar float
struct O { float4 p [[position]]; device float* s [[point_size]]; };
vertex O f(uint vid [[vertex_id]], device float* in [[buffer(0)]]) { O o; o.p = float4(0.0); o.s = in; return o; }
