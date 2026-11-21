// EXPECT: valid
// DISASM-MATCH: OpConstantComposite %v4uchar %uchar_44(_[0-9]+)? %uchar_255(_[0-9]+)? %uchar_255(_[0-9]+)? %uchar_0(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v2short %short_n32768(_[0-9]+)? %short_1(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v2uint %uint_4294967295(_[0-9]+)? %uint_4294967295(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v2int %int_1(_[0-9]+)? %int_n2(_[0-9]+)?[^0-9_]
//
// Each component converts to the vector's own type as at run time: an integer wraps to the
// width, a float truncates toward zero. Apple folds the same values.
constant uchar4 kNarrow = uchar4(300, -1, 255, 256);
constant short2 kShort = short2(32768, 65537);
constant uint2 kNegative = uint2(-1);
constant int2 kTruncated = int2(1.9f, -2.9f);

kernel void file_scope_constant_int_vector_wraps(device int *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int(kNarrow.x) + int(kShort.x) + int(kNegative.y) + kTruncated.y;
}
