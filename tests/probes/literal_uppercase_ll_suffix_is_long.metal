// EXPECT: valid
// DISASM: = OpConstant %long 1

//
// LL is the capital spelling of ll.
constant long kValue = 1LL;

kernel void literal_uppercase_ll_suffix_is_long(device long* out [[buffer(0)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
