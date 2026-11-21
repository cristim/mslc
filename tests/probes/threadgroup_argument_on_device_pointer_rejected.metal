// EXPECT: error has [[threadgroup(n)]]
//
// Apple: the attribute needs a threadgroup pointer.
kernel void threadgroup_argument_on_device_pointer_rejected(device float* a [[threadgroup(0)]]) { a[0] = 1.0; }
