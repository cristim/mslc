// EXPECT: valid
// DISASM-MATCH: OpLoad %v4float %[A-Za-z0-9_]+ Aligned 16
// A constant T& buffer parameter used as a value reads the whole buffer value.
#include <metal_stdlib>
using namespace metal;
fragment float4 reference_parameter_read_as_value(constant float4 &color [[buffer(0)]]) {
  float4 x = color;
  return x;
}
