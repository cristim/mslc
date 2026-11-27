// EXPECT: error invalid digit in integer constant "09"
//
// 9 is not an octal digit. The literal was read as 0.
kernel void literal_octal_digit_rejected(device int* out [[buffer(0)]],
                                         uint i [[thread_position_in_grid]])
{
    out[i] = 09;
}
