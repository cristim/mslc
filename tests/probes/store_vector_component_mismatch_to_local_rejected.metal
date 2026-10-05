// EXPECT: error a vector is not implicitly converted to a vector of another component type
//
// The same for an assignment to a local.
kernel void store_vector_component_mismatch_to_local_rejected(device float2 *out [[buffer(0)]], constant int2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float2 r = float2(0.0);
    r = v[i];
    out[i] = r;
}
