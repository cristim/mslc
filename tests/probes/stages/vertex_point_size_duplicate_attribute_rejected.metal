// EXPECT: error more than one [[point_size]] attribute
struct O { float4 p [[position]]; float s [[point_size, point_size]]; };
vertex O f(uint vid [[vertex_id]]) { O o; o.p = float4(0.0); o.s = 1.0; return o; }
