uint32_t _rng_state = millis();

#define RNG_MAGIC 75380540

// Roughly: https://en.wikipedia.org/wiki/Xorshift
inline void _permute_rng_state() {
	uint32_t x = _rng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	_rng_state = x;
}

void init_rng() {
  _rng_state = RNG_MAGIC - millis();
  for (int i=0; i<101; i++)
	_permute_rng_state();
}

// A small hack to use all the entropy
unsigned char increment = 0;
inline uint8_t random_unit8(){
	if (increment == 0) 
		_permute_rng_state();

	increment = (increment + 1) % 4;
	return _rng_state >> (increment * 8);
}