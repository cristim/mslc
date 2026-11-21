// EXPECT: valid
// DISASM: OpReturnValue %int_1002
// DISASM-NOT: OpReturnValue %int_1001
//
// Apple: the using declaration is in an enclosing block of the use.
#include <metal_stdlib>
using namespace metal;
namespace E { constant int v = 1001; }
namespace N { enum E { v = 1002 }; }
int g(int x) { using N::E; { return E::v; } }
kernel void block_using_enum_in_nested_block_hides_outer_namespace(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = g(int(i)); }
