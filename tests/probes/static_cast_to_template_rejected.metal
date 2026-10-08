// EXPECT: error a cast to a pointer, reference or template type is not supported
//
// texture2d is a template name, not a cast target.
kernel void static_cast_to_template_rejected(texture2d<float> t [[texture(0)]], device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    static_cast<texture2d<float>>(t);
    out[i] = 0.0f;
}
