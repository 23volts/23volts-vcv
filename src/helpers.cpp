#include "helpers.hpp"
#include "rack.hpp"

int randomInteger(int min, int max) {
	return std::floor(rack::random::uniform() * (max + 1) - min) + min;
}

int randomNormal() {
	return abs(rack::random::normal() / 5.f);
}

bool hasDecimals(float number) {
	return (number - (int) number) > 0.f;
}

float round3(float number) {
	return (float) std::round(number * 100) / 100;
}

int rangeToIndex(float value, int size, float rangeMin, float rangeMax) {
	float range = rangeMax - rangeMin;
	float ratio = value / range;
	return std::ceil(ratio * size) - 1;
}