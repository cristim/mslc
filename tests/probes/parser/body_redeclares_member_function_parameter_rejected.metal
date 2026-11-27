// EXPECT: error redefinition of "x"
//
// Apple: "redefinition of 'x'".
#include <metal_stdlib>
using namespace metal;
struct S { int a; int f(int x) const { int x = 2; return x + a; } };
kernel void body_redeclares_member_function_parameter_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s; s.a = 1; o[i] = s.f(1); }
