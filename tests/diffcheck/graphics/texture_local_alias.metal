// RUN: vertex=vs fragment=fs cases=3
// DIFFCHECK-POSITION: pos
// A texture local, an auto, an alias of an alias, one in a nested block that hides the outer name,
// one passed to a helper, and a sampler copy: each reads the texture it names. The two textures
// hold different seeded texels and each term has its own weight, so an alias bound to the other
// texture, or to nothing, changes the colour. The reflection cross-check says no alias got a
// binding of its own.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; float4 col [[attribute(1)]]; };
struct VOut { float4 pos [[position]]; float4 col; };
vertex VOut vs(VIn in [[stage_in]]) {
	VOut o;
	o.pos = in.pos;
	o.col = in.col;
	return o;
}
float4 look(texture2d<float> t, sampler u) { return t.sample(u, float2(0.375, 0.625)); }
fragment float4 fs(VOut in [[stage_in]], texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]],
	sampler s [[sampler(0)]]) {
	texture2d<float> b = t1;
	auto c = b;
	texture2d<float> d = c;
	sampler u = s;
	auto v = u;
	float4 r = in.col + d.sample(v, float2(0.375, 0.625)) * float4(1.0, 2.0, 3.0, 4.0);
	{
		texture2d<float> b = t0;
		r += b.sample(u, float2(0.125, 0.875)) * float4(0.5, 0.25, 0.125, 0.0625);
	}
	return r + look(c, v) * float4(0.0078125, 0.015625, 0.03125, 0.0625) + b.sample(s, float2(0.625, 0.125));
}
