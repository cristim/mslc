// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpMatrixTimesVector %v4float
// DISASM-CAPTURE: (%[0-9]+) = OpFNegate %v4float
// DISASM: OpMatrixTimesMatrix
// DISASM-NO-MATCH: OpDecorate %[0-9]+ NoContraction
//
// Apple emits llvm.fmuladd for matrix*vector and matrix*matrix (fusing is allowed) and fneg is exact, so
// neither is decorated (#371). The only NoContraction-free module is this one, so a
// decoration added to either operation fails here.
kernel void safe_math_matrix_vector_and_negate_stay_undecorated(device float4 *out [[buffer(0)]],
    device const float4 *a [[buffer(1)]], device const float4x4 *m [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = -((*m) * (*m) * a[i]);
}
