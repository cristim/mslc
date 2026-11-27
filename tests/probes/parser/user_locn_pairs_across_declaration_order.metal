// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "user_locn_pairs_across_declaration_order"
//
// The fragment's struct lists its fields in the other order and leaves one out.
// Each field is at the Location its locnN name gives it in both modules, so the
// pair agrees where declaration order alone would not.
struct VOut {
    float4 p [[position]];
    float4 a [[user(locn0)]];
    float2 b [[user(locn1)]];
    float4 c [[user(locn2)]];
};

struct FIn {
    float4 p [[position]];
    float4 c [[user(locn2)]];
    float4 a [[user(locn0)]];
};

vertex VOut user_locn_pairs_across_declaration_order_vertex(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.a = float4(1.0);
    o.b = float2(1.0);
    o.c = float4(1.0);
    return o;
}

fragment float4 user_locn_pairs_across_declaration_order(FIn in [[stage_in]])
{
    return in.a + in.c;
}
