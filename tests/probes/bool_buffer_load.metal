// EXPECT: valid
// DISASM-MATCH: OpLoad %uchar %[0-9]+ Aligned 1
// DISASM-MATCH: OpINotEqual %bool %[0-9]+ %uchar_0
// DISASM: OpCapability StorageBuffer8BitAccess
//
// Metal gives a bool one byte in a buffer. SPIR-V gives OpTypeBool no layout,
// so the buffer holds bytes and a load compares the byte with zero.
kernel void bool_buffer_load(device uint *out [[buffer(0)]], device const bool *flags [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (flags[i]) {
        out[i] = 1u;
    }
}
