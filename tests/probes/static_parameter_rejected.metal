// EXPECT: error a parameter cannot be static
kernel void static_parameter_rejected(static uint3 i [[thread_position_in_grid]]) {}
