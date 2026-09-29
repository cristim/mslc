// Metal pads a 3-component vector out to a 4-component register, so a float3
// occupies 16 bytes and aligns to 16, where the three floats in it are 12. Every
// layout number a float3 produces follows from that, and there are three of
// them, one per binding here:
//
//   - the member of a struct, which starts at the next multiple of 16, so the
//     float2 after the float3 below starts at 32 rather than 28;
//   - the element of an array, whose ArrayStride is 16, which is also the
//     smallest stride Vulkan's relaxed storage buffer layout admits;
//   - the member of a constant block, the same stride in the storage class that
//     additionally refuses a 12-byte one.
//
// The fixture is a kernel rather than a graphics stage so that the layout is the
// only thing in it: nothing is rasterised, so every float it reads is a float
// this layout put where the host wrote it.
//
// The host reference for these numbers is Iridium's, which translates the same
// Metal to SPIR-V that indium's own tests run on a GPU: a float3 array there
// carries ArrayStride 16, and a float3x3 is an array of three float3 rather
// than a matrix, which is a difference of representation and not of size.
struct Padded
{
    float4 whole;
    float3 narrow;
    float2 pair;
};

kernel void float3_layout(device float3 *vectors [[buffer(0)]],
                          device Padded *mixed [[buffer(1)]],
                          constant float3 *uniforms [[buffer(2)]],
                          uint index [[thread_position_in_grid]])
{
    vectors[index] = uniforms[index];
    mixed[index].whole = float4(mixed[index].narrow, mixed[index].pair.x);
    mixed[index].pair = mixed[index].narrow.xy;
}
