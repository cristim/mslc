// EXPECT: valid
// A directive ends at the end of its own line. Skipping to the next '#', as
// mslc used to, consumed the kernel below and left "source declares no kernel,
// vertex or fragment entry point".
#define SCALE 4
#define SQUARE(x) ((x) * (x))
kernel void define_then_kernel(device uint* out [[buffer(0)]],
                               uint i [[thread_position_in_grid]])
{ out[i] = i; }
