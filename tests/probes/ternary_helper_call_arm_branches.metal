// EXPECT: valid
// DISASM: = OpPhi %float
// DISASM: = OpFunctionCall %float
// DISASM-NOT: = OpSelect 
// DISASM-BRANCH-FALLTHROUGH: OpSelectionMerge
//
// A helper may read anything, so a call in an arm is not evaluated unless that
// arm is chosen: the call sits in its own block and the arms join in an OpPhi.
float twice(float x)
{
    return x * 2.0f;
}
kernel void ternary_helper_call_arm_branches(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    bool c = i > 3;
    out[i] = c ? twice(1.5f) : 0.0f;
}
