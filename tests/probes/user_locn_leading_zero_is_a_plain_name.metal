// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 0
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 1
// DISASM-NO-MATCH: OpDecorate %[A-Za-z0-9_]+ Location 2
//
// Only the canonical spelling locnN (no leading zero) claims Location N. Apple
// treats locn01 and locn1 as different names, so mslc does too: locn01 is a plain
// name and takes the first free Location (0), locn1 claims 1.
struct VOut {
    float4 p [[position]];
    float4 a [[user(locn01)]];
    float4 b [[user(locn1)]];
};

vertex VOut user_locn_leading_zero_is_a_plain_name(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.a = float4(1.0);
    o.b = float4(1.0);
    return o;
}
