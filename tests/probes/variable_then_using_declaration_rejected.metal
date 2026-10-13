// EXPECT: error the using declaration of "A::x" conflicts with "x" declared earlier in the same namespace
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
constant int x = 2;
using A::x;
