// EXPECT: valid
// DISASM: OpIAdd
// Two headers that include each other are fine when each is guarded.
#include "include/pp_guarded_cycle_a.h"
kernel void pp_guarded_include_cycle_terminates(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = PP_CYCLE_A + PP_CYCLE_B; }
