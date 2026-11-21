// RUN: vertex=vs fragment=fs
// DIFFCHECK-POSITION: pos
// Three colour targets of three types. The set of targets comes from this source, not from mslc's
// module, so an output mslc drops or retypes is a reported difference, not a missing comparison.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; };
vertex float4 vs(VIn in [[stage_in]]) { return float4(in.pos, 0.5, 1.0); }
struct FOut { float4 a [[color(0)]]; int2 b [[color(1)]]; uint c [[color(2)]]; };
fragment FOut fs() {
	FOut o;
	o.a = float4(0.25, 0.5, 0.75, 1.0);
	o.b = int2(-3, 4);
	o.c = 7u;
	return o;
}
