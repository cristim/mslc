// EXPECT: error call to "f" is ambiguous
// The same signature in two namespaces a using directive makes visible is ambiguous (Apple).
#include <metal_stdlib>
using namespace metal;
namespace A { float f(int x) { return 1.0f; } }
namespace B { float f(int x) { return 2.0f; } }
using namespace A;
using namespace B;
kernel void k(device float* out [[buffer(0)]])
{ out[0] = f(1); }
