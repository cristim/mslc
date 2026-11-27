// EXPECT: error "i" is a variable, not a constant
// The condition has to be a constant expression.
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { static_assert(i > 0, "x"); out[0] = 1; }
