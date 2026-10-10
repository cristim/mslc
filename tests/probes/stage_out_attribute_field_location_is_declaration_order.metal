// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9]+ Location 0
// DISASM-NO-MATCH: Location 3
//
// [[attribute(n)]] names a vertex input; Apple accepts it on a vertex
// function's returned struct too, where it has no vertex-fetch meaning. The
// field gets its declaration-order Location like any other output field
// ("c" is the first field after "position", so it lands on Location 0), not
// its attribute index; a regression that used the index instead would put
// it at Location 3.
struct Out {
    float4 p [[position]];
    float4 c [[attribute(3)]];
};

vertex Out stage_out_attribute_field_location_is_declaration_order(uint vid [[vertex_id]])
{
    Out o;
    o.p = float4(1.0);
    o.c = float4(0.0);
    return o;
}
