// EXPECT: error "N::x" conflicts with the using declaration of "A::x" in the same namespace
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace N { using A::x; constant int x = 2; }
