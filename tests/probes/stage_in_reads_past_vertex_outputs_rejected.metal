// EXPECT: error have to be the vertex function's first ones
//
// The fragment reads a field "b" that the vertex function never writes, so its
// Location 1 would have no writer. Comparing the two lists as a prefix must not
// read past the end of the shorter vertex list; the module is reported instead.
struct VOut {
    float4 p [[position]];
    float4 a;
};

struct FIn {
    float4 p [[position]];
    float4 a;
    float4 b;
};

vertex VOut stage_in_reads_past_vertex_outputs_vertex(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.a = float4(0.5);
    return o;
}

fragment float4 stage_in_reads_past_vertex_outputs_rejected(FIn in [[stage_in]])
{
    return in.b;
}
