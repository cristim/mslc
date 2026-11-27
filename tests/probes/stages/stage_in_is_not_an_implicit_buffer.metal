// EXPECT: valid
// REFLECT: "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "member": 0, "param_index": 1, "name": "tint"
//
// A buffer parameter without [[buffer(n)]] takes its index from its position
// among the parameters that bind, and a [[stage_in]] parameter is not one of
// them: it is the interface, not a buffer. Counting it would give "tint" Metal
// index 1, and indium would bind the wrong buffer there.
struct In {
    float4 position [[position]];
    float4 color;
};

fragment float4 stage_in_is_not_an_implicit_buffer(In in [[stage_in]], constant float4* tint)
{
    return in.color * tint[0];
}
