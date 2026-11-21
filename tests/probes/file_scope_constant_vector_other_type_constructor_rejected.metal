// EXPECT: error has to be a float4 already
//
// "constant float4 c = half4(1);" and "= float3(1)" are not a float4 constructor; mslc converts no
// column and refuses them rather than fold a wrong vector.
constant float4 c = half4(1);
kernel void file_scope_constant_vector_other_type_constructor_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = c.x;
}
