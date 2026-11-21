// EXPECT: error a long is not an index
// Apple rejects a bare sizeof as an attribute index: its type is unsigned long.
kernel void sizeof_attribute_index_ulong_rejected(device float *out [[buffer(sizeof(float))]])
{
    out[0] = 1.0f;
}
