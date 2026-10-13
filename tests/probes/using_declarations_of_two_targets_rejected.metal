// EXPECT: error the using declaration of "B::x" conflicts with the using declaration of "A::x" in the same namespace
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
namespace B { constant int x = 22; }
using A::x;
using B::x;
