// EXPECT: error redefined differently
// C makes an incompatible redefinition an error; Apple's compiler warns.
#define SIZE 1
#define SIZE 2
kernel void pp_redefinition_differs_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
