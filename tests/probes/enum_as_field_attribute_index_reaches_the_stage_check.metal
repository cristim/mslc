// EXPECT: error [[stage_in]] on a vertex function is not lowered yet
//
// The index of [[attribute(n)]] is an enumerator here. What this pins is that the attribute list parses and the source gets as far as the stage check; a vertex stage_in is not lowered yet, so that is the answer.
#include <metal_stdlib>
using namespace metal;
enum Attr { Position, Color = 2 };
struct In { float4 position [[attribute(Position)]]; float4 color [[attribute(Color)]]; };
struct Out { float4 position [[position]]; };
vertex Out enum_as_field_attribute_index_reaches_the_stage_check(In in [[stage_in]])
{ Out o; o.position = in.position + in.color; return o; }
