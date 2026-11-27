// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v2float
//
// The same for an assignment to a local vector.
kernel void store_scalar_broadcast_to_local_vector(device float2 *out [[buffer(0)]], constant float *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float2 r = float2(0.0);
    r = v[i];
    out[i] = r;
}
