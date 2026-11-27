// EXPECT: error cannot store through "k"
//
// Apple: "read-only variable is not assignable"; a const pointee is not writable.
kernel void threadgroup_argument_const_write_rejected(const threadgroup float* k [[threadgroup(0)]],
                                                      uint l [[thread_position_in_threadgroup]])
{ k[l] = 1.0; }
