// EXPECT: error variables in function scope cannot be in the device address space
kernel void for_init_address_space_rejected(device float* o [[buffer(0)]])
{ for (int device i = 0; i < 2; i++) { o[i] = 1.0; } }
