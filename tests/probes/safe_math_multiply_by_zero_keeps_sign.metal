// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DIFFCHECK-RANGE: k -4 4
// DISASM-ENTRY-MODES: safe_math_multiply_by_zero_keeps_sign LocalSizeId % % %, SignedZeroInfNanPreserve 32
//
// x * 0 is -0 for a negative x. Without SignedZeroInfNanPreserve a NIR driver
// folds it to +0 (#368). The mode is what this probe pins; the run proves it.
kernel void safe_math_multiply_by_zero_keeps_sign(device float *out [[buffer(0)]],
    device const int *k [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float a = float(k[i]) - 0.5f;
    out[i] = a * 0.0f;
}
