// EXPECT: valid
// DISASM: %int_11
// A conditional of two enumerators of namespace A is a value of that enum, so A::f is found for it.
#include <metal_stdlib>
using namespace metal;
namespace A { enum E { X, Y }; int f(E e) { return 11; } }
kernel void k(device int* out [[buffer(0)]]) {
	out[0] = f(out[1] > 0 ? A::X : A::Y);
}
