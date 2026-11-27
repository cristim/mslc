// EXPECT: error is a reference, which is not lowered yet
//
// Apple accepts threadgroup T& [[threadgroup(n)]]; mslc refuses it until a caller needs one.
kernel void threadgroup_argument_reference_rejected(threadgroup float& a [[threadgroup(0)]]) { a = 1.0; }
