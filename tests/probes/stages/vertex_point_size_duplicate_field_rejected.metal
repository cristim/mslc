// EXPECT: error more than one [[point_size]] field
struct O { float4 p [[position]]; float a [[point_size]]; float b [[point_size]]; };
vertex O f(uint vid [[vertex_id]]) { O o; o.p = float4(0.0); o.a = 1.0; o.b = 1.0; return o; }
