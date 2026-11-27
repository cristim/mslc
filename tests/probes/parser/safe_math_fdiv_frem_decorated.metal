// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpFDiv %float
// DISASM-CAPTURE: (%[0-9]+) = OpFRem %float
// DISASM-MATCH-CAPTURED: OpDecorate $1 NoContraction
// DISASM-MATCH-CAPTURED: OpDecorate $2 NoContraction
//
// Apple's safe math emits a plain fdiv and fmod with no fast-math flags, so a NIR
// driver must not turn the divide into a reciprocal multiply or fold the remainder (#371).
kernel void safe_math_fdiv_frem_decorated(device float *out [[buffer(0)]],
    device const float *a [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = a[i] / a[i + 1] + fmod(a[i], a[i + 2]);
}
