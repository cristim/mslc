// EXPECT: valid
// DISASM: OpFunctionCall
// DISASM-MATCH: OpIAdd %int %[_0-9a-zA-Z]+ %int_20[_0-9]*
// DISASM-NOT: OpKill
#include <metal_stdlib>
struct S { static int f(int value) { return value + 70; } };
namespace metal { struct S { static int f(int value) { return value + 20; } }; }
kernel void k(device int* out [[buffer(0)]], const device int* input [[buffer(1)]]) {
    out[0] = metal::S::f(input[0]);
}
