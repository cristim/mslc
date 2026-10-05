// EXPECT: error resolves outside the directories it may include from
// A name that leaves the root is refused before the filesystem is asked whether it exists, so the answer does not tell.
#include "../not_there_anywhere.h"
kernel void pp_include_escaping_the_root_when_absent_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
