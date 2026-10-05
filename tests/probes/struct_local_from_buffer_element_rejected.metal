// EXPECT: error is initialised from another struct, or from one read straight from a buffer
//
// A struct read from a buffer has the buffer's member offsets in its SPIR-V type
// and a local does not, so the two are different types. Storing one into the
// other used to emit an OpBitcast between structs, which spirv-val rejects after
// mslc reported success.
struct V {
    float4 p;
    float4 c;
};

kernel void struct_local_from_buffer_element_rejected(const device V* x [[buffer(0)]],
                                                      device float4* y [[buffer(1)]],
                                                      uint i [[thread_position_in_grid]])
{
    V o = x[i];
    y[i] = o.c;
}
