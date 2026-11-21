// EXPECT: error has [[threadgroup(n)]]
//
// A parameter takes one of [[buffer]] and [[threadgroup]].
kernel void threadgroup_argument_with_buffer_index_rejected(threadgroup float* a [[threadgroup(0), buffer(1)]]) { a[0] = 1.0; }
