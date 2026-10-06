// EXPECT: valid
// DISASM: = OpConstant %long 6000000000

//
// The int on the left is converted to the long on the right, so the product is
// not taken at 32 bits.
constant long kValue = 2 * 3000000000;

kernel void file_scope_constant_wider_operand_wins(device long* out [[buffer(0)]],
                                                   uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
