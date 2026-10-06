// EXPECT: error mslc does not support more than one address space on a type, found "constant" after another
//
// "device constant Vertex *" names one buffer, so which of the two was meant
// decides which storage class its binding lands in. Resolving it would mean
// picking one silently, so the second address space is reported.
kernel void two_address_spaces_rejected(device constant uint *in [[buffer(0)]],
                                        device uint *out [[buffer(1)]],
                                        uint i [[thread_position_in_grid]])
{
    out[i] = in[i];
}
