// EXPECT: error variables in function scope cannot be in the constant address space
kernel void local_constant_qualifier_rejected(device float* o [[buffer(0)]]) { constant float x = 1.0; o[0] = x; }
