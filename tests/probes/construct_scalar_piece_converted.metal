// EXPECT: valid
// DISASM-MATCH: = OpConvertFToS %int %float_1_5[^ _0-9a-zA-Z]
// DISASM-MATCH: = OpCompositeConstruct %v4int %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^ _0-9a-zA-Z]
//
// A scalar piece is converted to the target's component type, as Apple does.
kernel void construct_scalar_piece_converted(device int4 *out [[buffer(0)]], constant int3 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = int4(v[i], 1.5);
}
