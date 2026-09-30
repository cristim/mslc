// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 32
//
// A buffer through a pointer parameter, in the spelling a vertex shader usually
// uses: the address space after the const qualifier, reached through "->", with
// a member read through an index.
//
// Three things had to hold at once. The address space and the qualifier are read
// in one loop now, because as two fixed sequences "const" was taken as the whole
// prefix and "device" was then read as a type name, so the parameter became
// "Vertex" and the list ended at the "*". "->" and "vertices[index].color" both
// had to be read, and an index is part of the access chain rather than the end
// of it: loading the element first hands OpAccessChain a value where it needs an
// address.
//
// A Metal "device T*" is a buffer of T, so the descriptor is a
// Block-decorated { T runtime_array[] } and the parameter indexes through that.
// The element is a third form of the struct: laid out with its offsets, but not
// Block-decorated, because Vulkan requires a struct nested inside a Block to be
// laid out and rejects a Block-decorated one inside an array. The stride pin is
// that array stepping by the struct's own size, 32 bytes for two float4s.
struct Vertex
{
    float4 position;
    float4 color;
};

struct Uniforms
{
    float4 scale;
};

kernel void pointer_parameters(device float4 *out [[buffer(0)]],
                               constant Uniforms *uniforms [[buffer(1)]],
                               const device Vertex *vertices [[buffer(2)]],
                               uint index [[thread_position_in_grid]])
{
    out[index] = uniforms->scale * vertices[index + 1].color;
}
