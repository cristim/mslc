// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpMatrixTimesScalar %mat4v4float
// DISASM-CAPTURE: (%[0-9]+) = OpVectorTimesMatrix %v4float
// DISASM-MATCH-CAPTURED: OpDecorate $1 NoContraction
// DISASM-MATCH-CAPTURED: OpDecorate $2 NoContraction
//
// Apple emits plain fmul for matrix*scalar and air.dot per column for vector*matrix,
// both without fast-math flags (#371).
kernel void safe_math_matrix_scalar_and_vector_matrix_decorated(device float4 *out [[buffer(0)]],
    device const float4 *a [[buffer(1)]], device const float4x4 *m [[buffer(2)]],
    device const float *s [[buffer(3)]], uint i [[thread_position_in_grid]])
{
    out[i] = a[i] * ((*m) * s[0]);
}
