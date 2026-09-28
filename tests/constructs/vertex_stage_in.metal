// A vertex entry point handed the attributes of the vertex it is drawing, which
// is what [[stage_in]] on a vertex function means.
//
// A fragment stage's [[stage_in]] is the interface it receives, and its
// [[position]] is FragCoord, the position the fragment is at. A vertex stage's
// is the other thing: [[attribute(n)]] says which vertex buffer field this is,
// and there is no FragCoord to substitute for a position the fragment stage
// would have received. mslc rejected the whole parameter on a vertex stage
// rather than reading the attributes, which is what every vertex function in
// the corpus that is not fed a device buffer writes.
//
// The two attributes are at 0 and 1 rather than at 0 and 1 of two different
// structs, so a swap or an off-by-one in the location would be visible: a
// member read at the wrong location is a member read from the wrong field.
struct Attributes
{
    float4 position [[attribute(0)]];
    float4 normal [[attribute(1)]];
};

struct Projected
{
    float4 position [[position]];
    float4 normal;
};

vertex Projected vertex_stage_in(Attributes vertex [[stage_in]],
                                 constant float4 *scale [[buffer(0)]],
                                 uint vid [[vertex_id]])
{
    Projected out;
    out.position = vertex.position * scale[0];
    out.normal = vertex.normal + vertex.position;

    return out;
}

// The fragment side of the same struct is the form mslc already read, and it is
// here so the two are compiled from one source: the interface has to agree from
// both ends.
fragment float4 fragment_stage_in(Projected interpolated [[stage_in]])
{
    return interpolated.normal;
}
