// EXPECT: error mslc does not support more than one address space on a type
//
// An mslc limitation: Apple accepts this spelling.
kernel void mixed_address_spaces_unsupported(device thread float* p [[buffer(0)]])
{ float x = p[0]; }
