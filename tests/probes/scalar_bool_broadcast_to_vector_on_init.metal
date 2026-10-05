// EXPECT: valid
// DISASM-MATCH: = OpSelect %float
// DISASM-MATCH: = OpCompositeConstruct %v3float
//
// Apple fills every component of a vector with a scalar initialiser, a bool
// included: "float3 r = flag;" is float3(float(flag)).
kernel void scalar_bool_broadcast_to_vector_on_init(device float3 *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool flag = v[i] > 3u;
    float3 r = flag;
    out[i] = r;
}
