// EXPECT: error parameter "i" takes a builtin by value, so it cannot be in the device address space
kernel void builtin_device_address_space_rejected(device uint3 i [[thread_position_in_grid]]) {}
