// EXPECT: valid
// DISASM: = OpConstant %long 0
// DISASM-NOT: = OpConstant %long 4294967296

//
// 4294967295u is the largest uint, so the sum wraps to 0 in 32 bits. Declared as a
// long, a value that kept the ulong would read 4294967296.
constant long kValue = 4294967295u + 1;

kernel void literal_unsigned_max_stays_uint(device long* out [[buffer(0)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
