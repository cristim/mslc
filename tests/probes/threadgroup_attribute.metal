// EXPECT: error unsupported attribute "threadgroup"
kernel void threadgroup_attribute(threadgroup float* scratch [[threadgroup(0)]],
                                  uint i [[thread_position_in_grid]])
{ scratch[i] = 0.0; }
