// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DIFFCHECK-RANGE: k 0 8
// DISASM-CAPTURE: (%[0-9]+) = OpConvertFToU %uint %[0-9]+
// DISASM-CAPTURE: (%[0-9]+) = OpConvertUToF %float %[0-9]+
// DISASM-CAPTURE: (%[0-9]+) = OpFMul %float %[^ ]+ %[^ ]+
// DISASM-MATCH-CAPTURED: OpDecorate $1 NoContraction
// DISASM-MATCH-CAPTURED: OpDecorate $2 NoContraction
// DISASM-MATCH-CAPTURED: OpDecorate $3 NoContraction
//
// The unsigned float conversions and the multiply carry NoContraction (#368). A
// readback cannot tell these apart from the folded form for non-negative inputs
// on lavapipe, so the decoration is pinned in the disassembly.
kernel void safe_math_unsigned_conversions_and_multiply_decorated(device float *out [[buffer(0)]],
    device const int *k [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float x = float(k[i]) * 0.25f;
    out[i] = float(uint(x));
}
