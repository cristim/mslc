// Calls a helper it only declares: the definition lives in link_definer.metal,
// and the two are joined at link time.
#include <metal_stdlib>
using namespace metal;
#include "shared.h"

kernel void quadruple(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[i] = scaled_twice(1.5);
}
