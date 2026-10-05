// EXPECT: error is returned by a vertex function and has no [[position]] field
//
// Apple rejects this too ("invalid return type"): a rasteriser has nothing to
// place without a position.
struct Out { float4 c; };

vertex Out vertex_output_without_position_rejected(uint vid [[vertex_id]])
{
    Out o;
    o.c = float4(1.0);
    return o;
}
