// EXPECT: valid
// DISASM: SAbs
// DISASM-NOT: OpReturnValue %int_1007
//
// Apple: the directive for metal comes first, so when it is looked up A has not been nominated yet.
#include <metal_stdlib>
namespace A { namespace metal { int abs(int a) { return 1007; } } }
using namespace metal; using namespace A;
int f(int x) { return abs(x); }
kernel void metal_directive_before_nominating_directive_is_the_library(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = f(int(i)); }
