// ++ and -- as statements are x += 1 and x -= 1.
kernel void step_int(device int *out [[buffer(0)]],
                     uint i [[thread_position_in_grid]])
{
    int x = out[i];
    x += 1;
    x += 1;
    x -= 1;
    x -= 1;
    out[i] = x;
}
