// EXPECT: valid
// A device T& parameter is assigned, compound-assigned and swizzle-assigned as a value.
#include <metal_stdlib>
using namespace metal;
kernel void device_reference_parameter_store(device float4 &o [[buffer(0)]], constant float4 &x [[buffer(1)]]) {
  o = x * 2.0;
  o.x = 1.0;
  o += x;
}
