// EXPECT: error is initialised from another struct, or from one read straight from a buffer
//
// The same copy on the way to a stage output: the local is the returned struct's
// own type, so emitReturn accepts it, and the bad copy is the initialiser.
struct V {
    float4 position [[position]];
    float4 color;
};

vertex V returned_local_from_buffer_element_rejected(const device V* vertices [[buffer(0)]],
                                                     uint vid [[vertex_id]])
{
    V o = vertices[vid];
    return o;
}
