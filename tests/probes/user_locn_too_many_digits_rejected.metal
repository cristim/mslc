// EXPECT: error names a Location too large to be one
//
// Nine digits are the most a locnN name may spell; ten are rejected.
struct VOut {
    float4 p [[position]];
    float4 a [[user(locn1000000000)]];
};

vertex VOut user_locn_too_many_digits_rejected(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(1.0);
    o.a = float4(1.0);
    return o;
}
