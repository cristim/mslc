// EXPECT: valid
// DISASM: SAbs
// DISASM-NOT: OpReturnValue %int_1007
//
// Apple: ::metal is looked up in the file scope, which declares it, so a directive is not consulted.
#include <metal_stdlib>
namespace A { namespace metal { int abs(int a) { return 1007; } } }
using namespace A;
int f(int x) { return ::metal::abs(x); }
kernel void global_qualified_metal_ignores_directive(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = f(int(i)); }
