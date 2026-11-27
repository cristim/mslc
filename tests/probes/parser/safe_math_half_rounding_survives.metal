// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DIFFCHECK-RANGE: k -1000 1000
// DISASM-CAPTURE: (%[0-9]+) = OpFConvert %half %[0-9]+
// DISASM-CAPTURE: (%[0-9]+) = OpFConvert %float %[0-9]+
// DISASM-MATCH-CAPTURED: OpDecorate $1 NoContraction
// DISASM-MATCH-CAPTURED: OpDecorate $2 NoContraction
// DISASM-ENTRY-MODES: safe_math_half_rounding_survives LocalSizeId % % %, SignedZeroInfNanPreserve 32, SignedZeroInfNanPreserve 16
//
// float(half(x)) rounds x to a half, and Metal's safe math keeps the rounding.
// Without NoContraction a NIR driver folds the pair to x (#368): 0.1f then reads
// back as 0.1 instead of 0.0999755859375.
kernel void safe_math_half_rounding_survives(device float *out [[buffer(0)]],
    device const int *k [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = float(half(float(k[i]) * 0.1f));
}
