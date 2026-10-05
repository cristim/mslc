// ++ and -- as statements are x += 1 and x -= 1.
kernel void step_int4(device int4 *out [[buffer(0)]],
                      uint i [[thread_position_in_grid]])
{
    int4 x = out[i];
    x++;
    --x;
    out[i] = x;
}
