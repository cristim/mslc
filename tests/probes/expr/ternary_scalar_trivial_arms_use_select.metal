// EXPECT: valid
// DISASM: = OpSelect %float
// DISASM-NOT: OpPhi
// DISASM-NOT: OpSelectionMerge
//
// Both arms are reads of locals, which cannot trap or touch memory, so evaluating
// both and selecting is the same as evaluating the chosen one.
kernel void ternary_scalar_trivial_arms_use_select(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float a = in[0];
    float b = in[1];
    out[i] = in[2] > 0.0f ? a : b * 2.0f;
}
