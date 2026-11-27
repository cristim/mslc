// EXPECT: error a parameter cannot be constexpr
kernel void constexpr_parameter_rejected(constexpr uint3 i [[thread_position_in_grid]]) {}
