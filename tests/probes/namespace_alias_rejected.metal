// EXPECT: error a namespace alias is not supported
// Apple accepts this; mslc rejects it rather than lower it.
#include <metal_stdlib>
using namespace metal;
namespace A { namespace B { constant float k = 1.0; } }
namespace X = A::B;
