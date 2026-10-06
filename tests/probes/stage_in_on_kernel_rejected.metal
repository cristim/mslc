// EXPECT: error [[stage_in]] on a kernel function is not lowered
//
// Apple accepts a [[stage_in]] struct on a kernel, but it has no vertex or
// fragment interface to be read from, so mslc reports it rather than treating
// it as a buffer.
struct In { float4 p; };

kernel void stage_in_on_kernel_rejected(In in [[stage_in]], device float4* out [[buffer(0)]])
{
    out[0] = in.p;
}
