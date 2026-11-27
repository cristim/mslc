// EXPECT: error a condition has to be a bool or a numeric scalar
//
// Apple: "value of type 'float3' is not contextually convertible to 'bool'".
kernel void ternary_vector_condition_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float3 v = float3(in[0]);
    out[i] = v ? 1.0f : 2.0f;
}
