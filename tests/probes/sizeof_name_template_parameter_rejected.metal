// EXPECT: error worked out after parsing
// Apple gives 4 for f(1.0f) and 2 for f(half(1)). A parameter of a template instance has no
// recorded type, so an array length from its size is refused.
template <typename T> float f(T x)
{
    float a[sizeof(x)];
    return a[0] + float(x);
}
kernel void sizeof_name_template_parameter_rejected(device float *out [[buffer(0)]])
{
    out[0] = f(1.0f);
}
