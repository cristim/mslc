// EXPECT: error index 31 is out of bounds
//
// Apple: "'threadgroup' attribute parameter is out of bounds: must be between 0 and 30".
kernel void threadgroup_argument_index_out_of_range_rejected(threadgroup float* a [[threadgroup(31)]], uint l [[thread_position_in_threadgroup]]) { a[l] = 1.0; }
