// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 3
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 1
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 0
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 2
// DISASM-NO-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 4
//
// [[user(locnN)]] is Location N. A plain field takes the first Location no
// locnN field claims, so the two here are 0 and 2, and neither collides with 1
// or 3.
struct VOut {
    float4 p [[position]];
    float2 a [[user(locn3)]];
    float4 b;
    float4 c [[user(locn1)]];
    float4 d;
};

vertex VOut user_locn_is_the_location(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.a = float2(1.0);
    o.b = float4(1.0);
    o.c = float4(1.0);
    o.d = float4(1.0);
    return o;
}
