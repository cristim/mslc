// EXPECT: error the arguments of pow have to be one type
// Two vectors of different types are not converted; Apple rejects the call too.
kernel void math_pow_of_mixed_vectors_rejected(
    device const float3* v [[buffer(0)]],
    device const half3* h [[buffer(1)]],
    device float3* out [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = pow(v[i], h[i]);
}
