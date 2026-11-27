// EXPECT: error "f" is not a builtin mslc recognises
// The directive holds only to the end of its block (Apple: use of undeclared identifier 'f').
#include <metal_stdlib>
using namespace metal;
namespace A { int f() { return 11; } }
kernel void using_directive_ends_with_block_rejected(device int* out [[buffer(0)]])
{
    { using namespace A; out[0] = f(); }
    out[1] = f();
}
