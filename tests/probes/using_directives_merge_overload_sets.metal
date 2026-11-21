// EXPECT: valid
// Functions of two namespaces a using directive makes visible are one overload set: f(1) is
// A::f(int) and f(1.0f) is B::f(float) (Apple).
#include <metal_stdlib>
using namespace metal;
namespace A { float f(int x) { return 1.0f; } }
namespace B { float f(float x) { return 2.0f; } }
using namespace A;
using namespace B;
kernel void k(device float* out [[buffer(0)]])
{ out[0] = f(1) + f(1.0f); }
