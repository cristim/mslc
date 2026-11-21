// EXPECT: error threadgroup argument "a" is not assignable
//
// Apple accepts reassigning the pointer; mslc refuses until a caller needs it. The argument names threadgroup memory of a length the host sets; it is not an array value, and the
// diagnostic must not print the placeholder length mslc keeps for it.
kernel void threadgroup_argument_assign_rejected(threadgroup float* a [[threadgroup(0)]], threadgroup float* b [[threadgroup(1)]])
{ a = b; }
