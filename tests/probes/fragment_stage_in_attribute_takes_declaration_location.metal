// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9a-z_]+ Location 1
// DISASM-NOT: Location 3
//
// Apple accepts [[attribute(n)]] on a fragment function's stage_in struct, where
// it has no meaning. The field takes its declaration-order Location, which is
// what a vertex function returning the same struct writes: 1 here, after b.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[position]]; float4 b; float4 c [[attribute(3)]]; };
fragment float4 fragment_stage_in_attribute_takes_declaration_location(In in [[stage_in]]) { return in.b + in.c; }
