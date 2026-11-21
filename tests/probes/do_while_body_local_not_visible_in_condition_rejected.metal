// EXPECT: error "k" is not a parameter
kernel void do_while_body_local_not_visible_in_condition_rejected(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    do {
        uint k = 1u;
        out[i] = k;
    } while (k < 3u);
}
