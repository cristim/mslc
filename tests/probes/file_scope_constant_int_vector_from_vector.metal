// EXPECT: valid
// DISASM-MATCH: OpConstantComposite %v4int %int_n294967296(_[0-9]+)? %int_n294967296(_[0-9]+)? %int_n294967296(_[0-9]+)? %int_n294967296(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v2uint %uint_4294967295(_[0-9]+)? %uint_2(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v2int %int_1(_[0-9]+)? %int_n2(_[0-9]+)?[^0-9_]
// DISASM-MATCH: OpConstantComposite %v3bool %true(_[0-9]+)? %false(_[0-9]+)? %true(_[0-9]+)?[^0-9_]
//
// A vector converted from a vector of another scalar type, as the whole argument list.
constant int4 kWrapped = int4(uint4(4000000000u));
constant uint2 kSigned = uint2(int2(-1, 2));
constant int2 kFloats = int2(float2(1.5f, -2.5f));
constant bool3 kBools = bool3(int3(5, 0, -1));

kernel void file_scope_constant_int_vector_from_vector(device int *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = kWrapped.x + int(kSigned.x) + kFloats.y + int(kBools.x);
}
