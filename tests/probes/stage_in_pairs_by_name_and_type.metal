// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "stage_in_pairs_by_name_and_type"
//
// "b" is a float4 in VA and a float2 in FB and VB. A field pairs by its name and
// its type together, so FB pairs with VB, where "b" leads, and not with VA, where
// a "b" sits second and would fail the leading-run rule. Pairing by name alone
// would reject this module, which Apple compiles.
struct VA {
    float4 p [[position]];
    float4 a;
    float4 b;
};

struct VB {
    float4 p [[position]];
    float2 b;
};

vertex VA stage_in_pairs_by_name_and_type_va(uint vid [[vertex_id]])
{
    VA o;
    o.p = float4(1.0);
    return o;
}

vertex VB stage_in_pairs_by_name_and_type_vb(uint vid [[vertex_id]])
{
    VB o;
    o.p = float4(1.0);
    return o;
}

fragment float4 stage_in_pairs_by_name_and_type(VB in [[stage_in]])
{
    float2 b = in.b;
    return float4(1.0);
}
