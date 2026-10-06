// EXPECT: valid
// DISASM-MATCH: = OpAccessChain %_ptr_Function_float %[0-9]+ %uint_1
// DISASM-MATCH: = OpFAdd %float
//
// v[1] += 1.0f changes lane 1 of the local in place. Apple accepts it; it
// used to be rejected here as "not lowered yet" (#53).
kernel void compound_local_vector_element(device float4 *out [[buffer(0)]],
                                          uint i [[thread_position_in_grid]])
{
    float4 v = out[i];
    v[1] += 1.0f;
    out[i] = v;
}
