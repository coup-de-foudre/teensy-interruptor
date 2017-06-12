uint32_t _rng_state = millis()

// Roughly: https://en.wikipedia.org/wiki/Xorshift
inline uint32_t tiny_prng() {
	uint32_t x = _rng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	_rng_state = x;
	return x;
}

void init_rng() {
  _rng_state = 75380540 - millis()
  for (int i=0; i<100; i++)
	tiny_prng()
}
