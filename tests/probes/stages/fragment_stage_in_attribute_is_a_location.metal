// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9]+ Location 1
// DISASM-NO-MATCH: Location 4
//
// Apple accepts [[attribute(n)]] on a fragment function's stage_in struct,
// where it has no vertex-fetch meaning; the field gets its declaration-order
// Location like any other crossing field, not its attribute index. "a" takes
// Location 0 (shared with the fragment output's own Location 0), so only "c"
// can land on Location 1; a regression that used the attribute index instead
// would put it at Location 4.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[position]]; float4 a; float4 c [[attribute(4)]]; };
fragment float4 fragment_stage_in_attribute_is_a_location(In in [[stage_in]]) { return in.a + in.c; }
