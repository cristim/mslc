// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_18[_0-9]* Aligned
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_3[_0-9]* Aligned
// A::K::y with K in Z, which A uses; A has a variable K, which a namespace name does not
// see; and through a directive to the file's namespace metal (Apple accepts; #279).
#include <metal_stdlib>
using namespace metal;
namespace Z { namespace K { constant int y = 18; } }
namespace A { constant int K = 5; using namespace Z; }
namespace metal { namespace L { constant int w = 3; } }
namespace B { using namespace metal; }
kernel void qualified_namespace_through_using_directive(device int* out [[buffer(0)]])
{ out[0] = A::K::y; out[1] = B::L::w; }
