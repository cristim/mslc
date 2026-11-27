// EXPECT: error reserves threadgroup index 0
//
// Apple: "cannot reserve 'threadgroup' resource location at index 0".
kernel void threadgroup_argument_duplicate_index_rejected(threadgroup float* a [[threadgroup(0)]], threadgroup float* b [[threadgroup(0)]], uint l [[thread_position_in_threadgroup]]) { a[l] = b[l]; }
