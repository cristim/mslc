// EXPECT: valid
// DISASM-MATCH: OpConstantComposite %v4int %int_1(_[0-9]+)? %int_1(_[0-9]+)? %int_1(_[0-9]+)? %int_1(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v4int %int_n1(_[0-9]+)? %int_2(_[0-9]+)? %int_3(_[0-9]+)? %int_4(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v4int %int_5(_[0-9]+)? %int_6(_[0-9]+)? %int_7(_[0-9]+)? %int_8(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v3uint %uint_9(_[0-9]+)? %uint_10(_[0-9]+)? %uint_11(_[0-9]+)?[^0-9_]
//
// File-scope integer vector constants built by their own constructor: the splat, the component
// list, and a vector piece beside scalars, as Apple folds them.
constant int4 kSplat = int4(1);
constant int4 kList = int4(-1, 2, 3, 4);
constant int4 kMixed = int4(int2(5, 6), 7, 8);
constexpr constant uint3 kUnsigned = uint3(9, uint2(10, 11));

kernel void file_scope_constant_int_vector_constructor(device int *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = kSplat.x + kList.x + kMixed.w + int(kUnsigned.z);
}
