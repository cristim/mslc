// A function declared here and defined in another unit, whose name is overloaded here, cannot be
// imported: the plain Linkage name would not say which overload the other unit exports (#266).
#include <metal_stdlib>
using namespace metal;

float scale(float x);
float scale(float3 x);

float use(float x) {
	return scale(x);
}
