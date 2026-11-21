// EXPECT: error cannot be in the device address space

#include <metal_stdlib>
using namespace metal;

fragment float4 framebuffer_fetch_device_address_space_rejected(device float4 c [[color(0)]]) { return c; }
