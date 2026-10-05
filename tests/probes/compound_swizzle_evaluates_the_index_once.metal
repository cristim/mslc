// EXPECT: valid
// DISASM-MATCH: = OpIMul %uint %[0-9]+ %uint_7
// DISASM-NO-MATCH: OpIMul %uint %[0-9]+ %uint_7.*OpIMul %uint %[0-9]+ %uint_7
// DISASM-NO-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_v4float .*OpAccessChain %_ptr_PhysicalStorageBuffer_v4float 
//
// out[i * 7u].xy += e computes the element's address once; the lanes are read from the one load
// and written back to the one address.
kernel void compound_swizzle_evaluates_the_index_once(device float4 *out [[buffer(0)]],
                                                      uint i [[thread_position_in_grid]])
{
    out[i * 7u].xy += float2(1.0f, 2.0f);
}
