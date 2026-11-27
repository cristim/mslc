// EXPECT: valid
// A ushort4 texel converted to int4 is a vector zero extension (the case that used to emit a scalar-result OpUConvert).
// DISASM-MATCH: OpUConvert %v4uint %[0-9A-Za-z_]+
// DISASM-NOT: OpUConvert %uint %v
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<ushort, access::read> t [[texture(0)]], device int4* o [[buffer(0)]]) { o[0] = int4(t.read(uint2(0))); }
