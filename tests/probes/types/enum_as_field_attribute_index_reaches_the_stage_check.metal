// EXPECT: valid
// DISASM: OpDecorate %12 Location 0
// DISASM: OpDecorate %14 Location 2
// DISASM: %12 = OpVariable %_ptr_Input_v4float Input
// DISASM: %14 = OpVariable %_ptr_Input_v4float Input
//
// The index of [[attribute(n)]] is an enumerator here: Position is 0 and Color
// is 2, so the Inputs sit at Locations 0 and 2 and nothing at 1.
#include <metal_stdlib>
using namespace metal;
enum Attr { Position, Color = 2 };
struct In { float4 position [[attribute(Position)]]; float4 color [[attribute(Color)]]; };
struct Out { float4 position [[position]]; };
vertex Out enum_as_field_attribute_index_reaches_the_stage_check(In in [[stage_in]])
{ Out o; o.position = in.position + in.color; return o; }
