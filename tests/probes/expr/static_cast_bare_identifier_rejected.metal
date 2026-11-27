// EXPECT: error "static_cast" is not a parameter, local, constant or builtin mslc knows about
//
// Without a '<' the identifier is no cast; it falls through to the ordinary
// name lookup and its ordinary diagnostic.
kernel void static_cast_bare_identifier_rejected(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = float(static_cast);
}
