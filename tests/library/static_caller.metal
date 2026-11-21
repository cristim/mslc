#include <metal_stdlib>
using namespace metal;
#include "shared_static.h"

kernel void run(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[i] = sq(ext(2.0));
}
