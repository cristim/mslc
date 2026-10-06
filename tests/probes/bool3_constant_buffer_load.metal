// EXPECT: valid
// DISASM-MATCH: OpINotEqual %bool %[0-9]+ %uchar_0[_0-9]*
// DISASM-NO-MATCH: OpINotEqual %bool %[0-9]+ %uchar_[1-9]
// DISASM-LANES: OpCompositeExtract %uchar 3
// DISASM-ASCENDING: OpCompositeConstruct %v3bool
//
// A constant buffer of bool3 is read like a device one: three lanes compared with zero.
kernel void bool3_constant_buffer_load(device uint *out [[buffer(0)]], constant bool3 *flags [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = uint(flags[i].z);
}
