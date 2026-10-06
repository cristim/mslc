// EXPECT: valid
// DISASM: = OpConstant %int 900000000

//
// Two shorts are promoted to int before they are multiplied, so 30000 * 30000 is
// 900000000 and not the short it would wrap to.
constant short kA = 30000;
constant int kValue = kA * kA;

kernel void file_scope_constant_short_operands_are_promoted(device int* out [[buffer(0)]],
                                                            uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
