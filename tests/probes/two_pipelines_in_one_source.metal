// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Vertex %[0-9]+ "two_pipelines_va"
// DISASM-MATCH: OpEntryPoint Vertex %[0-9]+ "two_pipelines_vb"
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "two_pipelines_fa"
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "two_pipelines_in_one_source"
//
// Two pipelines in one source: va feeds fa and vb feeds fb. Apple pairs a vertex
// output with a fragment input by name and type, so fa reading "color" is not a
// partner of vb, which writes only "uv", and the module is not rejected for it.
struct A {
    float4 p [[position]];
    float4 color;
};

struct B {
    float4 p [[position]];
    float2 uv;
};

vertex A two_pipelines_va(uint vid [[vertex_id]])
{
    A o;
    o.p = float4(1.0);
    o.color = float4(0.5);
    return o;
}

vertex B two_pipelines_vb(uint vid [[vertex_id]])
{
    B o;
    o.p = float4(1.0);
    o.uv = float2(0.5);
    return o;
}

fragment float4 two_pipelines_fa(A in [[stage_in]])
{
    return in.color;
}

fragment float4 two_pipelines_in_one_source(B in [[stage_in]])
{
    float2 uv = in.uv;
    return float4(1.0);
}
