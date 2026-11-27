// EXPECT: valid
// Vector integer conversions keep the vector result type at every width and signedness; widening across a change of signedness extends in the source's signedness first (ushort4 to int4 is a zero extension), narrowing and float/bool conversions are per component.
// DISASM-MATCH: OpUConvert %v4uint %[0-9A-Za-z_]+
// DISASM-MATCH: OpSConvert %v4int %[0-9A-Za-z_]+
// DISASM-MATCH: OpUConvert %v4ushort %[0-9A-Za-z_]+
// DISASM-MATCH: OpSConvert %v4short %[0-9A-Za-z_]+
// DISASM-NOT: OpUConvert %uint %v
// DISASM-NOT: OpSConvert %int %v
#include <metal_stdlib>
using namespace metal;
kernel void hit(device ushort4* us [[buffer(0)]], device short4* ss [[buffer(1)]], device int4* is [[buffer(2)]], device uint4* ui [[buffer(3)]],
    device float4* fs [[buffer(4)]], device bool4* bs [[buffer(5)]], device uchar4* uc [[buffer(6)]], device char4* sc [[buffer(7)]]) {
  is[0] = int4(us[0]);
  ui[0] = uint4(us[0]);
  ui[1] = uint4(ss[0]);
  is[1] = int4(ss[0]);
  us[1] = ushort4(is[0]);
  us[2] = ushort4(ui[0]);
  ss[1] = short4(is[0]);
  ss[2] = short4(ui[0]);
  is[2] = int4(uc[0]);
  is[3] = int4(sc[0]);
  uc[1] = uchar4(is[0]);
  sc[1] = char4(ui[0]);
  fs[0] = float4(us[0]) + float4(ss[0]);
  us[3] = ushort4(fs[0]);
  bs[0] = bool4(us[0]);
  is[4] = int4(bs[0]);
}
