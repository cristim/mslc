// EXPECT: error conflicts with "x" declared earlier in the same scope
//
// Apple: "target of using declaration conflicts with declaration already in scope". The outermost body block shares the parameters' scope.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 1; }
int h(int x) { using A::x; return x; }
kernel void block_using_redeclares_parameter_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = h(1); }
