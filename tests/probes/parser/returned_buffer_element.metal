// EXPECT: valid
// DISASM-NO-MATCH: OpBitcast %_struct
// DISASM-NO-MATCH: OpCompositeConstruct %_struct
//
// A struct read straight from a buffer is returned. Its SPIR-V type carries the
// buffer's member offsets, so it is copied into the plain form first. Apple
// accepts it.
struct V {
    float4 position [[position]];
    float4 color;
};

vertex V returned_buffer_element(const device V* vertices [[buffer(0)]],
                                 uint vid [[vertex_id]])
{
    return vertices[vid];
}
