// EXPECT: valid
// mslc has no warning channel; Apple's compiler continues past #warning, and so does mslc.
#warning this is only a warning
#warning don't mind the apostrophe
kernel void pp_warning_directive_accepted(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
