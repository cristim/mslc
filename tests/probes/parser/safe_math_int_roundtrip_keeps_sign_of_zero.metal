// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DIFFCHECK-RANGE: k -4 4
// DISASM-CAPTURE: (%[0-9]+) = OpConvertFToS %int %[0-9]+
// DISASM-CAPTURE: (%[0-9]+) = OpConvertSToF %float %[0-9]+
// DISASM-MATCH-CAPTURED: OpDecorate $1 NoContraction
// DISASM-MATCH-CAPTURED: OpDecorate $2 NoContraction
//
// float(int(x)) for a negative fraction is +0, not -0. A NIR driver folds the
// pair to trunc(x), which is -0, unless both conversions are NoContraction (#368).
kernel void safe_math_int_roundtrip_keeps_sign_of_zero(device float *out [[buffer(0)]],
    device const int *k [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float x = float(k[i]) * 0.25f;
    out[i] = float(int(x));
}
