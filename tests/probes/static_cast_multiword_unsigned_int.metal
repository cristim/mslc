// EXPECT: valid
// DISASM-MATCH: = OpBitcast %uint
//
// A multi-word target and a same-width signedness change, which is a
// reinterpretation, not a conversion.
kernel void static_cast_multiword_unsigned_int(device uint* out [[buffer(0)]], constant int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = static_cast<unsigned int>(in[i]);
}
