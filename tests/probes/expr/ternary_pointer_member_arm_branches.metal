// EXPECT: valid
// DISASM: = OpPhi %float
// DISASM-NOT: = OpSelect 
// DISASM-BRANCH-FALLTHROUGH: OpSelectionMerge
//
// A load through a pointer is not hoisted above the condition: sp may be unbound when c is false, so the arm gets its own block.
struct S { float a; };
kernel void ternary_pointer_member_arm_branches(device float* out [[buffer(0)]], device S* sp [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool c = i > 3;
    out[i] = c ? sp->a : 0.0f;
}
