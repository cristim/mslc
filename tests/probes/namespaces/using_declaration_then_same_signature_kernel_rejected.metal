// EXPECT: error "f" conflicts with the using declaration of "A::f" in the same namespace
#include <metal_stdlib>
using namespace metal;
namespace A { float f() { return 1.0; } }
using A::f;
kernel void f() { }
