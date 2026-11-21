// EXPECT: valid
// An enumerator is exactly its enum, and otherwise an int that promotes to int: f(ea) is
// f(E), f(1) is f(int), and g(int)/g(float) take an enum as an int (Apple).
#include <metal_stdlib>
using namespace metal;
enum E { ea, eb };
float f(E x) { return 1.0f; }
float f(int x) { return 2.0f; }
float g(int x) { return 3.0f; }
float g(float x) { return 4.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	const E e = eb;
	out[i] = f(ea) + f(e) + f(1) + g(e) + g(ea);
}
