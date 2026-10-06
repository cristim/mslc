// EXPECT: valid
// DISASM: = OpConstant %long 4294967295
//
// An int beside a uint is converted to the uint, so -1 + 0u is 4294967295.
constant long kValue = -1 + 0u;

kernel void file_scope_constant_signed_beside_unsigned_is_unsigned(device long* out [[buffer(0)]],
                                                                   uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
