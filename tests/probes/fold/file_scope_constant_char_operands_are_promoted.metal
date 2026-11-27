// EXPECT: valid
// DISASM: = OpConstant %int 200
//
// Two chars are promoted to int before they are added, so 100 + 100 is 200 and
// not the char -56 it would wrap to if the sum kept the operands' type.
constant char kHundred = 100;
constant int kValue = kHundred + kHundred;

kernel void file_scope_constant_char_operands_are_promoted(device int* out [[buffer(0)]],
                                                           uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
