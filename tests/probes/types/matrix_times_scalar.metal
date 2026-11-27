// EXPECT: valid
// DISASM-MATCH: OpMatrixTimesScalar %mat4v4float %[0-9]+ %float_2[^0-9_]
// DISASM-MATCH: OpMatrixTimesScalar %mat4v4float %[0-9]+ %float_0_5[^0-9_]
//
// SPIR-V has one opcode with the matrix first, so a scalar on the left is
// swapped into the second operand.
kernel void matrix_times_scalar(device float4 *out [[buffer(0)]],
                                device const float4x4 *m [[buffer(1)]],
                                device const float4 *in [[buffer(2)]],
                                uint i [[thread_position_in_grid]])
{
    float4x4 doubled = m[0] * 2.0;
    float4x4 halved = 0.5 * m[1];
    out[i] = doubled * in[i] + halved * in[i];
}
