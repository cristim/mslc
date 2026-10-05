// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Vertex %[0-9]+ "stage_in_reads_leading_vertex_outputs_vertex" %gl_VertexIndex %gl_Position %[0-9]+ %[0-9]+
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "stage_in_reads_leading_vertex_outputs" %gl_FragCoord %[0-9]+ %[0-9]+
// DISASM-NO-MATCH: "stage_in_reads_leading_vertex_outputs" %gl_FragCoord %[0-9]+ %[0-9]+ %[0-9]+
//
// A fragment function may read fewer fields than the vertex function writes, as
// long as they are the vertex function's first ones: "color" is Location 0 on
// both sides, and the vertex function's "uv" at Location 1 has no reader, which
// Vulkan allows. Apple accepts the pair. Only an order that would cross the
// Locations is reported (stage_interfaces_disagree_rejected).
struct VOut {
    float4 position [[position]];
    float4 color;
    float2 uv;
};

struct FIn {
    float4 position [[position]];
    float4 color;
};

vertex VOut stage_in_reads_leading_vertex_outputs_vertex(uint vid [[vertex_id]])
{
    VOut o;
    o.position = float4(1.0);
    o.color = float4(0.5);
    o.uv = float2(0.25);
    return o;
}

fragment float4 stage_in_reads_leading_vertex_outputs(FIn in [[stage_in]])
{
    return in.color;
}
