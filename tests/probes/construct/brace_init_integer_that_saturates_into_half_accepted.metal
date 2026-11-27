// EXPECT: valid
// DISASM: = OpConstant %half 0x1p+16

//
// Apple converts the half back to an int to see whether the value changed, and
// that conversion saturates: INT_MAX is infinity as a half and comes back as
// INT_MAX. 65536 does not, and is rejected. This is Apple's rule, followed here.
constant half2 kValue = { 2147483647, 1 };

kernel void brace_init_integer_that_saturates_into_half_accepted(device half* out [[buffer(0)]],
                                                                 uint i [[thread_position_in_grid]])
{
    out[i] = kValue.x;
}
