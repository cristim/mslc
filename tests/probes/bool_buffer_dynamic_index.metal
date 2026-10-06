// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_uchar %[0-9]+ %uint_0[_0-9]* %[0-9]+
//
// A bool buffer indexed by a run-time value is an access chain to the byte.
kernel void bool_buffer_dynamic_index(device uint *out [[buffer(0)]], device const bool *flags [[buffer(1)]], device const uint *which [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = uint(flags[which[i]]);
}
