// EXPECT: valid
// DISASM: OpReturnValue %int_15
// After a block's "using namespace A;", "using namespace B2;" in the same block finds
// A::B2 (Apple accepts).
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; namespace B2 { int g() { return 15; } } }
kernel void using_directive_in_block_nominates_nested_namespace(device int* out [[buffer(0)]])
{
    using namespace A;
    using namespace B2;
    out[0] = g();
}
