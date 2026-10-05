// EXPECT: valid
// DISASM-MATCH: OpLoad %v3float %[0-9]+ Aligned 4
// DISASM-MATCH: OpStore %[0-9]+ %[0-9]+ Aligned 4
// DISASM-NO-MATCH: Aligned 16
//
// The 12 bytes of a packed_float3 may start at any multiple of 4, so neither
// access may claim the 16 a float3 would.
kernel void packed_load_is_aligned_to_the_component(device packed_float3 *out [[buffer(0)]],
                                                    device const packed_float3 *in [[buffer(1)]],
                                                    uint index [[thread_position_in_grid]])
{
    out[index] = in[index];
}
