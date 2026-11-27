// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v2float
//
// A scalar stored into a vector buffer element fills every component, as Apple does.
kernel void store_scalar_broadcast_to_buffer_vector(device float2 *out [[buffer(0)]], constant float *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = v[i];
}
