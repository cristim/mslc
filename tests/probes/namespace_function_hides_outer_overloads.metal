// EXPECT: valid
// A function of an inner namespace hides the outer ones of that name instead of overloading
// them: inside N, f(1.0f) is N::f(float3) (a splat), not the file's f(float) (Apple).
#include <metal_stdlib>
using namespace metal;
float f(float x) { return 1.0f; }
namespace N {
	float f(float3 x) { return 2.0f; }
	float g() { return f(1.0f); }
}
kernel void k(device float* out [[buffer(0)]])
{ out[0] = N::g() + f(1.0f); }
