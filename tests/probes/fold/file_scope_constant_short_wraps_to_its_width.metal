// EXPECT: valid
// DISASM: = OpConstant %short -1
//
// A constant is converted to its declared type, and a short keeps the low 16 bits
// of 65535, sign-extended into the word as SPIR-V requires.
constant short kValue = 65535;

kernel void file_scope_constant_short_wraps_to_its_width(device short* out [[buffer(0)]],
                                                         uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
