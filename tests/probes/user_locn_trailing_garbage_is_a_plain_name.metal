// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 0
// DISASM-NOT: Location 1
//
// locn1x is not locnN: it is a plain name, so the field takes Location 0.
struct VOut {
    float4 p [[position]];
    float4 a [[user(locn1x)]];
};

vertex VOut user_locn_trailing_garbage_is_a_plain_name(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.a = float4(1.0);
    return o;
}
