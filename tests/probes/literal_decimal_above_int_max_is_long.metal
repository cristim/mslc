// EXPECT: valid
// DISASM: = OpConstant %long 3000000000
// DISASM-NOT: = OpConstant %uint 3000000000
// DISASM-NOT: = OpConstant %int -1294967296
//
// An unsuffixed decimal literal is the first of int and long that holds it, so
// 3000000000 is a long. It was an int whose 32 bits were read back as 3000000000
// for a uint and as -1294967296 anywhere it widened.
kernel void literal_decimal_above_int_max_is_long(device long* out [[buffer(0)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = 3000000000;
}
