// EXPECT: error the operand of unary '*' has to be a pointer parameter
//
// That is *(out++), pointer arithmetic, which is not lowered.
kernel void step_pointer_arithmetic_rejected(device int *out [[buffer(0)]],
                                             uint i [[thread_position_in_grid]])
{
    *out++;
}
