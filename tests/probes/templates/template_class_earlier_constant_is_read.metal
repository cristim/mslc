// EXPECT: valid
// Refs #341: names declared above the template are read; declarations below it that nothing in the template names are harmless.
#include <metal_stdlib>
using namespace metal;
constant int K = 3;
typedef float F;
template <typename T, int M = K> struct P { T v[M]; F f; int n() const { return K + M; } };
constant int K2 = 9;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	P<float> p;
	p.v[2] = 1.0f; p.f = 2.0f;
	out[i] = p.v[2] + p.f + float(p.n()) + float(K2);
}
