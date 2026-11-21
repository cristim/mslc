// Defines the helper that link_caller.metal declares through shared.h.
#include <metal_stdlib>
using namespace metal;
#include "shared.h"

float scaled_twice(float value) {
	return value * 2.0 * 2.0;
}
