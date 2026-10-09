// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9a-z_]+ Location 0
// DISASM-NOT: Location 3
//
// [[attribute(n)]] names a vertex input. Apple accepts it on a returned struct's
// field, where it has no meaning: the stages pair fields by name. The field
// takes the next Location in declaration order like any other, not n.
struct Out {
    float4 p [[position]];
    float4 c [[attribute(3)]];
};

vertex Out stage_out_attribute_field_takes_declaration_location(uint vid [[vertex_id]])
{
    Out o;
    o.p = float4(1.0);
    o.c = float4(0.0);
    return o;
}
