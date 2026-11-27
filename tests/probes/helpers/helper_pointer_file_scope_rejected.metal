// EXPECT: error argument 1 of the call to "read" is "data", which names a file-scope object, and a file-scope object is not a buffer
#include <metal_stdlib>
using namespace metal;
constant float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
float read(device float* p, uint i) { return p[i]; }
kernel void helper_pointer_file_scope_rejected(device float* out [[buffer(0)]],
                                               device float* data [[buffer(1)]],
                                               uint i [[thread_position_in_grid]])
{
    out[i] = read(::data, i);
}
