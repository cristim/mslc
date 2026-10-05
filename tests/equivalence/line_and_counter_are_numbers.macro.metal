#line 500
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = __LINE__ + __COUNTER__ + __COUNTER__; }
