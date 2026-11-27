// RUN: vertex=vs fragment=fs elems=1024 cases=2
// DIFFCHECK-POSITION: pos
// Writable [[buffer]] arguments in both vertex (indexed by vertex_id) and fragment (indexed by
// covered pixel coordinate, single instance, disjoint shapes in DEFAULT_GEOMETRY), reading from
// untouched tail elements so both seeded inputs and shader writes are checked.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; uint tag [[attribute(1)]]; };
struct VOut { float4 pos [[position]]; uint vval [[flat]]; };
struct VRec { float4 pos; uint idx; int tag; };
vertex VOut vs(VIn in [[stage_in]], uint vid [[vertex_id]],
               constant uint *scale [[buffer(0)]],
               device VRec *vout [[buffer(1)]]) {
	uint mix = (scale[vid] & 255u) + uint(vout[vid + 16u].tag & 255);
	vout[vid].pos = float4(in.pos, float(vid) * 0.25f, 1.0f);
	vout[vid].idx = vid * 7u + mix;
	vout[vid].tag = int(in.tag & 1023u) - int(vid);
	VOut o;
	o.pos = float4(in.pos, 0.5f, 1.0f);
	o.vval = mix;
	return o;
}
fragment uint4 fs(VOut in [[stage_in]],
                  device uint4 *fout [[buffer(0)]]) {
	uint x = uint(in.pos.x);
	uint y = uint(in.pos.y);
	uint p = y * 32u + x;
	uint4 seed = fout[960u + (p & 31u)];
	uint4 val = uint4(x + 1u, y + 3u, in.vval ^ p, (seed.x ^ seed.w) + x * 13u + y * 5u);
	fout[p] = val;
	return val;
}
