// EXPECT: valid
// Refs #341: a struct argument deduces U; a struct result that does not depend on U is read as a value.
#include <metal_stdlib>
using namespace metal;
struct P { float v; };
struct S {
	float a;
	template <typename U> float get(U x) const { return a + x.v; }
	template <typename U> P make(U x) const { P r; r.v = a + float(x); return r; }
	template <typename U> U twice(U x) const { return x + x; }
};
P mk(float x) { P p; p.v = x; return p; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	S s{1.0f};
	P p = mk(2.0f);
	P q = s.make(3);
	float3 w = s.twice(float3(1.0f, 2.0f, 3.0f));
	out[i] = s.get(p) + q.v + s.make(1).v + w.x + w.z + float(s.twice(2)) + S{2.0f}.get(p);
}
