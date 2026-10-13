// EXPECT: error the using declaration of "A::f" conflicts with "f" declared earlier in the same namespace
#include <metal_stdlib>
using namespace metal;
namespace A { float f() { return 1.0; } }
float f() { return 2.0; }
using A::f;
