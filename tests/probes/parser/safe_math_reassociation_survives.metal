// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DIFFCHECK-RANGE: k -1000 1000
// DISASM-CAPTURE: (%[0-9]+) = OpFAdd %float %[0-9]+ %float_1
// DISASM-CAPTURE: (%[0-9]+) = OpFSub %float %[0-9]+ %float_1
// DISASM-MATCH-CAPTURED: OpDecorate $1 NoContraction
// DISASM-MATCH-CAPTURED: OpDecorate $2 NoContraction
//
// (y + 1) - 1 is not y: the add rounds y away. Apple computes the IEEE result
// and a NIR driver re-associates it to y unless both are NoContraction (#368).
kernel void safe_math_reassociation_survives(device float *out [[buffer(0)]],
    device const int *k [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float y = float(k[i]) * 1.0e-5f;
    out[i] = (y + 1.0f) - 1.0f;
}
