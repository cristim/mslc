// EXPECT: error "x" is declared and cannot also be a namespace
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
using A::x;
namespace x { constant int y = 1; }
