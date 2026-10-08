// EXPECT: valid
static uint first_over(uint start, uint limit)
{
    uint r = 0u;
    for (uint j = start; j < 16u; j = j + 1u) {
        if (j <= limit) continue;
        r = j;
        break;
    }
    return r;
}
kernel void break_in_helper_function(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = first_over(2u, 5u);
}
