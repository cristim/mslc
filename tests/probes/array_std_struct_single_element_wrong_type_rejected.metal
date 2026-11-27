// EXPECT: error cannot initialize "Pair" from "Other"
#include <metal_stdlib>
using namespace metal;
struct Pair { float x; float y; };
struct Other { float a; float b; };
kernel void k(device float* out [[buffer(0)]]) {
	array<Pair, 1> a = { Other{ 1.0f, 2.0f } };
	out[0] = a[0].x;
}
