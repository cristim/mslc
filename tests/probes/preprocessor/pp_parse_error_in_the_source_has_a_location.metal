// EXPECT: error pp_parse_error_in_the_source_has_a_location.metal:5:5: unexpected
// The line and column are those of the token the parser stopped at.
struct Declared { float a; };

    int x;
kernel void pp_parse_error_in_the_source_has_a_location(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
