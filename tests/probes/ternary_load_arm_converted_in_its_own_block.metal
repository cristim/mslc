// EXPECT: valid
// DISASM-ORDER: = OpConvertSToF %float
// DISASM-ORDER: = OpPhi %float
//
// The int arm loads memory, so the arms branch, and the conversion to the common
// float type happens before the arm joins the Phi, in the arm's own block.
kernel void ternary_load_arm_converted_in_its_own_block(device float* out [[buffer(0)]], constant int* ints [[buffer(1)]], constant uint* n [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = i < n[0] ? ints[i] : 0.5f;
}
