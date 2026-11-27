// EXPECT: valid
kernel void do_while_nested_in_for_and_switch(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint sum = 0u;
    for (uint a = 0u; a < 3u; ++a) {
        switch (a) {
            case 0u:
                do { sum = sum + 1u; if (sum > 5u) break; } while (sum < 3u);
                break;
            default:
                do {
                    uint k = 0u;
                    do { k = k + 1u; } while (k < 2u);
                    sum = sum + k;
                } while (sum < 9u);
                break;
        }
    }
    out[i] = sum;
}
