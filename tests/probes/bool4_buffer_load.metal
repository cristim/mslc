// EXPECT: valid
// DISASM-MATCH: OpCompositeExtract %uchar %[0-9]+ 0
// DISASM-MATCH: OpCompositeExtract %uchar %[0-9]+ 1
// DISASM-MATCH: OpCompositeExtract %uchar %[0-9]+ 2
// DISASM-MATCH: OpCompositeExtract %uchar %[0-9]+ 3
// DISASM-MATCH: OpINotEqual %bool %[0-9]+ %uchar_0[_0-9]*
// DISASM-NO-MATCH: OpINotEqual %bool %[0-9]+ %uchar_[1-9]
// DISASM-ASCENDING: OpCompositeConstruct %v4bool
//
// Four lanes, each compared with zero in lane order.
kernel void bool4_buffer_load(device uint *out [[buffer(0)]], device const bool4 *flags [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (flags[i].w) {
        out[i] = 1u;
    }
}
