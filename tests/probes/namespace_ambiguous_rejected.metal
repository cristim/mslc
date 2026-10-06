// EXPECT: error reference to "k" is ambiguous
#include <metal_stdlib>
using namespace metal;
namespace A { constant float k = 1.0; }
namespace B { constant float k = 2.0; }
using namespace A;
using namespace B;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = k; }
