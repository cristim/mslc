// EXPECT: error a scalar cannot be converted to a vector
//
// Apple: "C-style cast from 'float3' to 'float' is not allowed".
kernel void cast_vector_to_scalar_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float3 v = float3(in[0]);
    out[i] = (float)v;
}
