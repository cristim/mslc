// EXPECT: valid
// DISASM: = OpPhi %float
// DISASM-NOT: = OpSelect 
// DISASM-BRANCH-FALLTHROUGH: OpSelectionMerge
//
// A member read through a reference parameter is a load, so it is guarded like one through a pointer.
struct S { float a; };
kernel void ternary_reference_member_arm_branches(device float* out [[buffer(0)]], device S& sr [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool c = i > 3;
    out[i] = c ? sr.a : 0.0f;
}
