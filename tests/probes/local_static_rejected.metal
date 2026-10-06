// EXPECT: error variables in function scope cannot be declared static
kernel void local_static_rejected(device float* o [[buffer(0)]]) { static float x = 1.0; o[0] = x; }
