// EXPECT: valid
// float c[] = {...} takes its length from the list.
kernel void local_array_deduced_length(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  float c[] = {10.0, 20.0, 30.0};
  int idx = int(in[0]);
  o[0] = c[idx + 1];
}
