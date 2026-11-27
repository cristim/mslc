// EXPECT: valid
// DISASM: OpFunctionCall
// DISASM: OpIAdd
#include <metal_stdlib>
int ghost(int value) { return value + 70; }
kernel void k(device int* out [[buffer(0)]], const device int* input [[buffer(1)]]) {
    out[0] = ghost(input[0]);
}
