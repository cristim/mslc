// EXPECT: error a conditional or comma expression as an argument chooses among the overloads
// Whether `c ? la : lb` is an lvalue (it binds f(thread float &)) or a prvalue (f(int)) depends on its arms;
// the ranking would take it for a thread prvalue and call the wrong function, so it is refused.
#include <metal_stdlib>
using namespace metal;
float f(thread float &x) { x = 9.0f; return 1.0f; }
float f(int x) { return 2.0f; }
kernel void k(device float *out [[buffer(0)]]) {
	bool c = out[5] > 0.0f; float la = 1.0f; float lb = 2.0f;
	out[0] = f(c ? la : lb);
}
