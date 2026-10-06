// EXPECT: valid
// DISASM: = OpConstant %long -9223372036854775807

//
// The largest long is a long and not a ulong, so dividing by -1 is signed.
constant long kValue = 0x7fffffffffffffff / -1;

kernel void literal_hex_long_max_stays_long(device long* out [[buffer(0)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
