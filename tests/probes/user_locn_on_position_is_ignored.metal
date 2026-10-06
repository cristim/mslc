// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 0
// DISASM-NOT: Location 1
//
// [[user(locn0)]] on the [[position]] field is ignored (Apple accepts it): it does
// not claim Location 0, so the plain field still takes 0.
struct VOut {
    float4 p [[position, user(locn0)]];
    float4 a;
};

vertex VOut user_locn_on_position_is_ignored(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.a = float4(1.0);
    return o;
}
