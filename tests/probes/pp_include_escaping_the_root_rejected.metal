// EXPECT: error resolves outside the directories it may include from
// ../run_probe.cmake exists, so this is the root check, not a missing file.
#include "../run_probe.cmake"
kernel void pp_include_escaping_the_root_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
