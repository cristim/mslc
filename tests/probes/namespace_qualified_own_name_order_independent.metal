// EXPECT: valid
// DISASM-MATCH: OpIAdd %int %int_33[_0-9]* %int_33[_0-9]*
//
// One namespace cannot list its own directives in both orders, so order
// independence needs two namespaces with the order reversed between them:
// A1 nominates B before C, A2 nominates C before B. Both resolve to C's own
// v (33), which a size-delta test on the shared accumulator (rather than a
// fresh local set per scope) can get wrong depending on which sibling is
// visited first, since B's using-declaration already copies C::v into the
// shared set before C is visited directly.
#include <metal_stdlib>
using namespace metal;
namespace D { constant int v = 44; }
namespace C { constant int v = 33; using namespace D; }
namespace B { using C::v; }
namespace A1 { using namespace B; using namespace C; }
namespace A2 { using namespace C; using namespace B; }
kernel void namespace_qualified_own_name_order_independent(device int *o [[buffer(0)]])
{
    o[0] = A1::v + A2::v;
}
