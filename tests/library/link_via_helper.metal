// An import reached only through a helper body, never from an entry point.
#include <metal_stdlib>
using namespace metal;

float shade(float value);

float via(float value) { return shade(value) + 1.0; }

kernel void run(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[i] = via(1.0);
}
