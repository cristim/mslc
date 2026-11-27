// EXPECT: valid
// DISASM: ArrayStride 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 32
//
// Metal pads a 3-component vector out to a 4-component register, so a float3
// occupies 16 bytes and aligns to 16, where the three floats in it are 12. Every
// layout number a float3 produces follows from that, and there are two of them
// here:
//
//   - the element of an array, whose ArrayStride is 16, which is also the
//     smallest stride Vulkan's relaxed storage buffer layout admits;
//   - the member of a struct, which starts at the next multiple of 16, so the
//     float2 after the float3 below starts at 32 rather than 28.
//
// A layout of 12 for the float3 is what mslc used to compute, and it validates:
// the offsets it produced were legal, the shader ran, and it read bytes the host
// had not put there. The pins are on the decorations rather than on a read-back
// because the decorations are the whole claim -- there is no indirect step
// between the type and the number written next to it -- and a read-back would
// re-derive what the pin already states.
//
// The host reference for these numbers is Iridium's, which translates the same
// Metal to the SPIR-V that indium's own tests run on a GPU: a float3 array there
// carries ArrayStride 16 too.
struct Padded
{
    float4 whole;
    float3 narrow;
    float2 pair;
};

kernel void float3_layout(device float3 *vectors [[buffer(0)]],
                          device float3 *out [[buffer(1)]],
                          device float2 *pairs [[buffer(2)]],
                          constant Padded &mixed [[buffer(3)]],
                          constant float3 *uniforms [[buffer(4)]],
                          uint index [[thread_position_in_grid]])
{
    vectors[index] = uniforms[index];
    out[index] = mixed.narrow * 2.0;
    pairs[index] = mixed.pair;
}
