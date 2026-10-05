// EXPECT: error has [[attribute(n)]], which mslc does not lower
//
// [[attribute(n)]] names a vertex input. Apple accepts it on a returned struct's
// field and the attribute has no meaning there, so mslc reports it rather than
// guessing whether the index was meant as the Location.
struct Out {
    float4 p [[position]];
    float4 c [[attribute(3)]];
};

vertex Out stage_out_attribute_field_rejected(uint vid [[vertex_id]])
{
    Out o;
    o.p = float4(1.0);
    o.c = float4(0.0);
    return o;
}
