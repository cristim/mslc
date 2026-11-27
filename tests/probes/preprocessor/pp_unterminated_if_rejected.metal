// EXPECT: error unterminated conditional directive
// The old conditional_is_an_error probe pinned a blanket refusal. Support replaced it, and this is what is left to refuse.
#if 1
kernel void pp_unterminated_if_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
