kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = (((i + 1u) * (i + 1u)) + (3u)); }
