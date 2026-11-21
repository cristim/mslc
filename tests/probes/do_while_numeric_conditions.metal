// EXPECT: valid
kernel void do_while_numeric_conditions(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    int n = 3;
    float f = 2.0f;
    uint sum = 0u;
    do { n = n - 1; sum = sum + 1u; } while (n);
    do { f = f - 1.0f; sum = sum + 1u; } while (f);
    out[i] = sum;
}
