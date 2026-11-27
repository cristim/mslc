// EXPECT: valid
// DISASM: OpReturnValue %int_1001
//
// Apple: a nested block is a new scope, so an alias there may reuse the name of a variable of the enclosing block. Only the outermost body block shares the parameters' scope.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int q = 1001; }
int h(int p) { int x = 0; { namespace x = A; return x::q; } }
kernel void namespace_alias_in_nested_block_hides_enclosing_local(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = h(1); }
