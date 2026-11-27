// EXPECT: error a pointer to a pointer is not supported
//
kernel void pointer_to_pointer_parameter_rejected(device float** out [[buffer(0)]])
{
}
