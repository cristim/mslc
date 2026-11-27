// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DIFFCHECK-RANGE: k 1 9
// DISASM-ENTRY-MODES: safe_math_x_minus_x_is_nan_for_inf LocalSizeId % % %, SignedZeroInfNanPreserve 32
//
// x + (-x) is NaN when x is infinite; a NIR driver folds it to 0 unless the add
// is NoContraction and infinities are preserved (#368). The readback shows the
// mode and the add; it does not show whether the multiplies need the decoration.
kernel void safe_math_x_minus_x_is_nan_for_inf(device float *out [[buffer(0)]],
    device const int *k [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float a = float(k[i]) * 1.0e30f;
    float b = float(k[i]) * 1.0e30f;
    float c = -(a * b);
    out[i] = a * b + c;
}
