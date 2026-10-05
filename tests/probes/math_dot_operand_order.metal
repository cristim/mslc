// EXPECT: valid
// OpDot takes dot's arguments in order. %13 is kAxis, pinned by the first
// needle; an OpDot that repeated either argument would pair %13 with itself
// in one of the two calls.
// DISASM: %13 = OpConstantComposite %v3float %float_0_5 %float_0_25 %float_2
// DISASM-MATCH: OpDot %float %[0-9]+ %13
// DISASM-MATCH: OpDot %float %13 %[0-9]+
// DISASM-NOT: OpDot %float %13 %13
constant float3 kAxis = { 0.5f, 0.25f, 2.0f };

kernel void math_dot_operand_order(
    device const float3* v [[buffer(0)]],
    device float* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = dot(v[i], kAxis) + dot(kAxis, v[i]);
}
