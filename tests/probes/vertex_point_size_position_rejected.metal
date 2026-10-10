// EXPECT: error incompatible point_size attribute combination
struct O { float4 p [[position, point_size]]; };
vertex O f(uint vid [[vertex_id]]) { O o; o.p = float4(0.0); return o; }
