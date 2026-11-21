// EXPECT: error worked out after parsing
// Apple gives 8 for sizeof of a texture parameter. A resource has no recorded type, so an array
// length from its size is refused, not sized as anything else.
kernel void sizeof_name_texture_parameter_rejected(device float *out [[buffer(0)]],
    texture2d<float> tex [[texture(0)]])
{
    float a[sizeof(tex)];
    out[0] = a[0];
}
