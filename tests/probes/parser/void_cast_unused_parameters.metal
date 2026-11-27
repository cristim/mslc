// EXPECT: valid
// (void)param; is the usual way to mark a parameter unused. A texture, a sampler, a buffer
// pointer and a buffer reference have no value to evaluate, so the cast only names them.
fragment float4 void_cast_unused_parameters(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]],
                                            device float *p [[buffer(0)]], constant float &c [[buffer(1)]]) {
  (void)t;
  (void)s;
  (void)p;
  (void)c;
  return float4(1.0);
}
