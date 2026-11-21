// EXPECT: valid
// DISASM: OpReturnValue %int_1001
//
// Apple: an alias in a nested block is a new scope, so it hides the parameter x and x::q is A's q.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int q = 1001; }
int h(int x) { { namespace x = A; return x::q; } }
kernel void namespace_alias_in_nested_block_hides_parameter(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = h(1); }
