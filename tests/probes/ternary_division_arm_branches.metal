// EXPECT: valid
// DISASM-BRANCH-FALLTHROUGH: OpSelectionMerge
// DISASM: = OpPhi %int
// DISASM-NOT: = OpSelect 
//
// An integer division can trap, so `d != 0 ? x / d : 0` evaluates it only when
// the divisor is not zero.
kernel void ternary_division_arm_branches(device int* out [[buffer(0)]], constant int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    int d = in[0];
    int x = in[1];
    out[i] = d != 0 ? x / d : 0;
}
