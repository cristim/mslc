// EXPECT: error has more than one [[position]] field
//
// One BuiltIn Position per entry point.
struct Out {
    float4 p [[position]];
    float4 q [[position]];
};

vertex Out two_position_fields_rejected(uint vid [[vertex_id]])
{
    Out o;
    o.p = float4(1.0);
    o.q = float4(1.0);
    return o;
}
