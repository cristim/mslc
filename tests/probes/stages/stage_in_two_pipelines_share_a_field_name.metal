// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "stage_in_two_pipelines_share_a_field_name_fa"
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "stage_in_two_pipelines_share_a_field_name_fb"
//
// Two genuinely separate pipelines share this file: VA/FA and VB/FB. Both
// declare a field named "c", but VA's is float4 and VB's is float2. Each
// fragment fully pairs with its own vertex by name and type, so neither
// should be rejected just because a different pipeline's field shares a name.
struct VA {
    float4 p [[position]];
    float4 c;
};

struct VB {
    float4 p [[position]];
    float2 c;
};

vertex VA stage_in_two_pipelines_share_a_field_name_va(uint vid [[vertex_id]])
{
    VA o;
    o.p = float4(1.0);
    o.c = float4(1.0);
    return o;
}

vertex VB stage_in_two_pipelines_share_a_field_name_vb(uint vid [[vertex_id]])
{
    VB o;
    o.p = float4(1.0);
    o.c = float2(1.0);
    return o;
}

fragment float4 stage_in_two_pipelines_share_a_field_name_fa(VA in [[stage_in]])
{
    return in.c;
}

fragment float4 stage_in_two_pipelines_share_a_field_name_fb(VB in [[stage_in]])
{
    return float4(in.c, 0.0, 1.0);
}
