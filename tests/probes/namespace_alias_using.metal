// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_31[_0-9]* Aligned
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_32[_0-9]* Aligned
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_33[_0-9]* Aligned
// A using directive or declaration through an alias works as through the namespace: at
// file scope, and in a block (Apple accepts).
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 31; constant int y = 32; constant int z = 33; }
namespace B = A;
using B::x;
kernel void namespace_alias_using(device int* out [[buffer(0)]])
{
    out[0] = x;
    { using namespace B; out[1] = y; }
    { using B::z; out[2] = z; }
}
