// EXPECT: valid
// DISASM: = OpPhi %float
// DISASM-NOT: = OpSelect 
// DISASM-BRANCH-FALLTHROUGH: OpSelectionMerge
//
// *p loads, so the arm branches.
struct S { float a; };
kernel void ternary_pointer_dereference_arm_branches(device float* out [[buffer(0)]], device float* p [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool c = i > 3;
    out[i] = c ? *p : 0.0f;
}
