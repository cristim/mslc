// EXPECT: valid
// Brace elision of typed Struct{...} and lower-rank array<Struct, 1>{...} elements inside 2D and 3D array<...>.
#include <metal_stdlib>
using namespace metal;
struct Pair { float x; float y; };
kernel void k(device float* out [[buffer(0)]]) {
	array<array<Pair, 1>, 1> nElided = { Pair{ 10.0f, 11.0f } };
	array<array<array<Pair, 1>, 2>, 1> n3d = { array<Pair, 1>{ Pair{ 1.0f, 2.0f } }, array<Pair, 1>{ Pair{ 3.0f, 4.0f } } };
	out[0] = nElided[0][0].x + nElided[0][0].y + n3d[0][0][0].x + n3d[0][1][0].y;
}
