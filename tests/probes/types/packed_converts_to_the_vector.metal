// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %v4float %[0-9]+ %[0-9]+
// DISASM-MATCH: OpFMul %v3float
// DISASM-MATCH: OpVectorShuffle %v2float %[0-9]+ %[0-9]+ 0 1
//
// A packed value is a float3 once it is loaded: it converts to one implicitly,
// builds a float4, takes a swizzle, and sits in arithmetic.
struct Packed
{
    packed_float3 direction;
    packed_float2 offset;
};

kernel void packed_converts_to_the_vector(device float4 *out [[buffer(0)]],
                                          device const Packed *in [[buffer(1)]],
                                          uint index [[thread_position_in_grid]])
{
    float3 direction = in[index].direction;
    packed_float3 scaled = direction * in[index].direction;
    float2 shifted = in[index].direction.xy + in[index].offset;
    out[index] = float4(scaled, shifted.x);
}
