// EXPECT: valid
// A runtime array cannot live in a Uniform block, so a constant pointer is a
// read-only storage buffer.
// DISASM: NonWritable
kernel void constant_pointer(constant float* scale [[buffer(0)]],
                             device float* out [[buffer(1)]],
                             uint i [[thread_position_in_grid]])
{ out[i] = out[i] * scale[i]; }
