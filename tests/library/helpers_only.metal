// A translation unit of helper functions and no entry point: a library of
// shared helpers compiled alongside the shaders that call them. It compiles to
// a Universal SPIR-V 1.5 Linkage object that exports each helper it defines,
// and default mode still refuses it, since a Vulkan executable needs an entry
// point.
#include <metal_stdlib>
using namespace metal;

float scaled(float value, float factor) {
	return value * factor;
}

float scaled_twice(float value) {
	return scaled(scaled(value, 2.0), 2.0);
}
