// EXPECT: error not allowed within a vertex function
//
// Apple rejects discard_fragment() in a vertex function, with this note:
//   note: function 'air.discard_fragment' is not allowed within a vertex function
// A vertex function has no fragment to drop, and OpKill there would kill an
// invocation in a stage that has none.
#include <metal_stdlib>
using namespace metal;
struct Out { float4 p [[position]]; };
vertex Out discard_fragment_in_a_vertex_function_is_an_error(uint vid [[vertex_id]])
{
    discard_fragment();
    Out o;
    o.p = float4(float(vid), 0.0, 0.0, 1.0);
    return o;
}
