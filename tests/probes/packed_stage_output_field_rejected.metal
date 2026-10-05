// EXPECT: error field "color" of "Output" is a packed_float3, which Apple's compiler does not allow across a stage boundary
struct Output
{
    float4 position [[position]];
    packed_float3 color;
};

vertex Output packed_stage_output_field_rejected()
{
    Output out;
    out.position = float4(0.0);
    out.color = float3(1.0);
    return out;
}
