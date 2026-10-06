// EXPECT: valid
// DISASM-ORDER: = OpConvertSToF %float
// DISASM-ORDER: = OpPhi %float
// DISASM-BRANCH-FALLTHROUGH: OpSelectionMerge
//
// The false arm loads memory, so the arms branch, and its conversion to the
// common float type is emitted in its own block, before the Phi that joins it.
kernel void ternary_false_arm_converted_in_its_own_block(device float* out [[buffer(0)]], constant int* ints [[buffer(1)]], constant uint* n [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = i < n[0] ? 0.5f : ints[i];
}
