// EXPECT: error reference to "f" is ambiguous
// A block's using directive puts A::f beside the file's f, so the call is ambiguous (Apple agrees);
// a using declaration would have hidden the file's f instead.
#include <metal_stdlib>
using namespace metal;
namespace A { int f() { return 11; } }
int f() { return 22; }
kernel void using_directive_in_block_ambiguous_rejected(device int* out [[buffer(0)]])
{
    using namespace A;
    out[0] = f();
}
