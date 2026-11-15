// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DIFFCHECK-RANGE: k -4 4
// DISASM-ENTRY-MODES: safe_math_add_zero_keeps_negative_zero LocalSizeId % % %, SignedZeroInfNanPreserve 32
//
// -0 + 0 is +0. A NIR driver folds x + 0 to x and returns -0 unless
// SignedZeroInfNanPreserve is set (#368). k == 0 gives a = -0.
kernel void safe_math_add_zero_keeps_negative_zero(device float *out [[buffer(0)]],
    device const int *k [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float a = float(k[i]) * -1.0f;
    out[i] = a + 0.0f;
}
