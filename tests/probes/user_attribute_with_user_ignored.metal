// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 3
//
// On a vertex input field [[user(...)]] is accepted by Apple and ignored by mslc:
// the input Location stays the [[attribute(3)]] index.
struct VIn {
    float4 a [[attribute(3), user(locn0)]];
};

struct VOut {
    float4 p [[position]];
};

vertex VOut user_attribute_with_user_ignored(VIn in [[stage_in]])
{
    VOut o;
    o.p = in.a;
    return o;
}
