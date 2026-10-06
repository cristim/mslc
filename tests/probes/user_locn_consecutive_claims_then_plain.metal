// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 2
// DISASM-NO-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 3
//
// Two claimed Locations in a row, then a plain field: it skips both and takes 2.
struct VOut {
    float4 p [[position]];
    float4 a [[user(locn0)]];
    float4 b [[user(locn1)]];
    float4 c;
};

vertex VOut user_locn_consecutive_claims_then_plain(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.a = float4(1.0);
    o.b = float4(1.0);
    o.c = float4(1.0);
    return o;
}
