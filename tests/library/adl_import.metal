// A function declared here and defined in another unit, which only argument-dependent lookup
// reaches, cannot be imported: the scan for imports reads the names a call spells (#266).
#include <metal_stdlib>
using namespace metal;

namespace A {
	struct S { float v; };
	float f(S s, int y);
}

float f(A::S s, float y) {
	return 2.0;
}

float use(A::S s) {
	return f(s, 1);
}
