// EXPECT: valid
// DISASM-NO-MATCH: OpBitcast %_struct
//
// The same copy on the way to a stage output: the local is the returned struct's
// own type, so emitReturn accepts it, and the initialiser copies the buffer's
// element into it member by member. Apple accepts it.
struct V {
    float4 position [[position]];
    float4 color;
};

vertex V returned_local_from_buffer_element(const device V* vertices [[buffer(0)]],
                                            uint vid [[vertex_id]])
{
    V o = vertices[vid];
    return o;
}
