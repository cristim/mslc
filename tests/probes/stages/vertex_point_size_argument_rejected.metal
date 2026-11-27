// EXPECT: error [[point_size]] takes no argument
struct O { float4 p [[position]]; float s [[point_size(1)]]; };
vertex O f(uint vid [[vertex_id]]) { O o; o.p = float4(0.0); o.s = 1.0; return o; }
