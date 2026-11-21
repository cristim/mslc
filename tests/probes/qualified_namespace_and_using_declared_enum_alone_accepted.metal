// EXPECT: valid
// DISASM: OpStore
// The controls of #321: the real namespace alone, and the imported enum alone,
// each still resolve (Apple stores 7 + 1).
#include <metal_stdlib>
using namespace metal;
namespace E { enum B { x, y }; }
namespace N { namespace B { constant int y = 7; } }
namespace M { using E::B; }
kernel void qualified_namespace_and_using_declared_enum_alone_accepted(device int* out [[buffer(0)]])
{
    out[0] = N::B::y + int(M::B::y);
}
