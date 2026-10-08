// EXPECT: valid
// DISASM: OpFunctionCall %uint
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
struct Writer { uint write(uint x) const { return x + 1u; } };
kernel void hit(device uint* out [[buffer(0)]], texture2d<uint, access::write> dst [[texture(2)]]) {
  Writer w; w.write(1u); uint value = w.write(0x80000000u); out[0] = value; dst.write(value, uint2(0));
}
