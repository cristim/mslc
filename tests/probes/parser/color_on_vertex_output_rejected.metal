// EXPECT: error [[color(n)]], which is not valid on a vertex output
struct V { float4 p [[position]]; float4 c [[color(0)]]; };
vertex V color_on_vertex_output_rejected() { V o; o.p = float4(0.0); o.c = float4(0.0); return o; }
