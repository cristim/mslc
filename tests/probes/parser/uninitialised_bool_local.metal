// EXPECT: valid
// A local with no initialiser is zero, and the zero of a bool is not a literal
// word: the grammar gives OpConstantFalse no literal operand at all, so
// OpConstant %bool 0 was an invalid instruction.
// DISASM: OpConstantFalse
// DISASM-NOT: OpConstant %bool
kernel void uninitialised_bool_local(device uint* out [[buffer(0)]],
                                    uint i [[thread_position_in_grid]])
{
    bool flag;
    if (flag) { out[i] = 1u; } else { out[i] = 2u; }
}
