#pragma once
kernel void pp_import_once(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 7373; }
