// EXPECT: error cannot honour #import <metal_nosuchheader>
#import <metal_nosuchheader>
kernel void metal_unknown_subheader_import_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
