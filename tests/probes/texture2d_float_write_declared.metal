// EXPECT: valid
// DISASM: OpCapability StorageImageWriteWithoutFormat
// DISASM: NonReadable
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::write> dst) {}
