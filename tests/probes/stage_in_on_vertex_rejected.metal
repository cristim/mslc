// EXPECT: error [[stage_in]] on a vertex function is not lowered yet
//
// A vertex function's [[stage_in]] is a set of vertex attributes read through
// a vertex descriptor, which is its own capability. It is reported rather than
// treated as a buffer.
struct In { float4 p [[attribute(0)]]; };
struct Out { float4 p [[position]]; };

vertex Out stage_in_on_vertex_rejected(In in [[stage_in]])
{
    Out o;
    o.p = in.p;
    return o;
}
