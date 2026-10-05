// EXPECT: error is [[position]], which has to be a float4
//
// BuiltIn Position and FragCoord are both four floats, so a position of any
// other type has no builtin to land in.
struct Out { float2 p [[position]]; };

vertex Out position_field_not_float4_rejected(uint vid [[vertex_id]])
{
    Out o;
    o.p = float2(1.0);
    return o;
}
