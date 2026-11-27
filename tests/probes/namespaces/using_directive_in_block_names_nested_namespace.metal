// EXPECT: valid
// DISASM: OpReturnValue %int_15
// A block's "using namespace A;" makes A's nested namespace B2 nameable as B2::g
// (Apple accepts).
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; namespace B2 { int g() { return 15; } } }
kernel void using_directive_in_block_names_nested_namespace(device int* out [[buffer(0)]])
{
    using namespace A;
    out[0] = B2::g();
}
