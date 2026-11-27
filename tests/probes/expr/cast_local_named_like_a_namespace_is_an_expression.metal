// EXPECT: valid
// DISASM-NOT: OpConvert
// DISASM: OpIAdd
//
// `(A) + 1` with a local int A and a namespace A is a parenthesised expression plus one,
// not a cast of +1 to the namespace's name.
#include <metal_stdlib>
using namespace metal;
namespace A { struct T { int v; }; }
kernel void cast_local_named_like_a_namespace_is_an_expression(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    int A = int(i);
    out[i] = (A) + 1;
}
