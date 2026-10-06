// EXPECT: valid
// DISASM-MATCH: = OpCompositeExtract %float %[0-9]+ 1
//
// Apple allows a packed vector to be subscripted, and so does mslc (#53).
struct Packed
{
    packed_float3 value;
};

kernel void packed_vector_subscript(device float *out [[buffer(0)]],
                                    device const Packed *in [[buffer(1)]],
                                    uint index [[thread_position_in_grid]])
{
    out[index] = in[index].value[1];
}
