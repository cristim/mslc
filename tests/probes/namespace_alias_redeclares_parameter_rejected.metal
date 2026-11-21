// EXPECT: error "x" is declared and cannot also be a namespace alias
//
// Apple: "redefinition of 'x' as different kind of symbol". The outermost body block shares the parameters' scope.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int q = 1; }
int h(int x) { namespace x = A; return 0; }
kernel void namespace_alias_redeclares_parameter_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = h(1); }
