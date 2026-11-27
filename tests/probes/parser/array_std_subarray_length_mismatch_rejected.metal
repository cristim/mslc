// EXPECT: error cannot initialize "Pair[2]" from "Pair[1]"
#include <metal_stdlib>
using namespace metal;
struct Pair { float x; float y; };
kernel void k(device float* out [[buffer(0)]]) {
	array<array<Pair, 2>, 1> a = { array<Pair, 1>{ Pair{ 1.0f, 2.0f } } };
	out[0] = a[0][0].x;
}
