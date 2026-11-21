// EXPECT: error is a threadgroup value
//
// Apple: "parameter may not be qualified with an address space". A threadgroup parameter is a
// pointer or a reference.
kernel void threadgroup_argument_value_rejected(device float* out [[buffer(0)]], threadgroup float x,
                                                uint l [[thread_position_in_threadgroup]])
{ out[l] = x; }
