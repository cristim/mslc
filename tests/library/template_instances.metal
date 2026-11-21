// Function templates in a library (#341): an instance is a function of the unit, never exported or
// imported (Apple makes it linkonce_odr), so only the helper that calls it has a Linkage name.
#include <metal_stdlib>
using namespace metal;

template <typename T> T twice(T x) {
	return x + x;
}

float uses_template(float x) {
	return twice(x) + float(twice(2));
}
