// EXPECT: error "S" conflicts with the using declaration of "A::S" in the same namespace
#include <metal_stdlib>
using namespace metal;
namespace A { struct S {}; int S() { return 1; } }
using A::S;
int S() { return 2; }
