// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 1[^0-9]
// DISASM-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-MATCH: OpConstant %ulong 16[^0-9]
// DISASM-NOT: OpFunction %uchar
// DISASM-NOT: OpFunction %half
// DISASM-NOT: OpFunction %v3float
//
// sizeof of a call is the size of the return type of the one function the name finds, for a
// scalar (uchar 1, half 2, int 4), a vector (float3 16) and a struct (8); the call is not made and
// the function is not emitted (n::size is the one that is called).
// A namespace's own f is the one an unqualified call inside the namespace finds (short, 2), not
// the file-scope f (int, 4). Apple compiles the same figures.
#include <metal_stdlib>
using namespace metal;
struct Pair { float a; int b; };
uchar one() { return 1; }
half two() { return 1.0h; }
int f() { return 1; }
float3 splat() { return float3(1.0f); }
Pair pair() { Pair p; p.a = 1.0f; p.b = 2; return p; }
namespace n {
    short f() { return 1; }
    ulong size() { return sizeof(f()); }
}
kernel void sizeof_expression_call(device ulong *out [[buffer(0)]])
{
    out[0] = sizeof(one());
    out[1] = sizeof(two());
    out[2] = sizeof(f());
    out[3] = sizeof(splat());
    out[4] = sizeof(pair());
    out[5] = n::size();
}
