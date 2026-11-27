// EXPECT: error a kernel returns void
//
// A kernel has no stage output for a value to go to; Apple rejects it too.
kernel float kernel_returning_a_value_rejected(device float* out [[buffer(0)]])
{
    return 1.0;
}
