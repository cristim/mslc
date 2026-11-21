// EXPECT: error "x" is declared and cannot also be a namespace alias
//
// Apple: "redefinition of 'x' as different kind of symbol". A for body shares the scope of the init declaration.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int q = 1; }
int h(int p) { for (int x = 0; x < 1; ++x) { namespace x = A; } return p; }
kernel void namespace_alias_redeclares_for_init_variable_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = h(1); }
