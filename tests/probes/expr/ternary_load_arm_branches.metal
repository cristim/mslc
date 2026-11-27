// EXPECT: valid
// DISASM-BRANCH-FALLTHROUGH: OpSelectionMerge
// DISASM: = OpPhi %float
// DISASM: OpBranchConditional
// DISASM-NOT: = OpSelect 
//
// buf[i] reads memory, so it may be evaluated only when the condition holds:
// `i < n ? buf[i] : 0.0f` is a bounds guard. The Phi form puts the load in its
// own block.
kernel void ternary_load_arm_branches(device float* out [[buffer(0)]], constant float* buf [[buffer(1)]], constant uint* n [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = i < n[0] ? buf[i] : 0.0f;
}
