// EXPECT: error variables in function scope cannot be in the device address space
kernel void local_device_qualifier_rejected(device float* o [[buffer(0)]]) { device float x = 1.0; o[0] = x; }
