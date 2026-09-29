// EXPECT: valid
// DISASM: OpShiftRightArithmetic
// DISASM: OpShiftRightLogical
kernel void shift_right_by_sign(device const int* sin [[buffer(0)]],
                                device int* sout [[buffer(1)]],
                                device const uint* uin [[buffer(2)]],
                                device uint* uout [[buffer(3)]],
                                uint i [[thread_position_in_grid]])
{
    sout[i] = sin[i] >> 1;
    uout[i] = uin[i] >> 1u;
}
