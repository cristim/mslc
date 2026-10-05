// ++ and -- as statements are x += 1 and x -= 1.
kernel void step_float(device float *out [[buffer(0)]],
                       uint i [[thread_position_in_grid]])
{
    float x = out[i];
    x++;
    --x;
    out[i] = x;
}
