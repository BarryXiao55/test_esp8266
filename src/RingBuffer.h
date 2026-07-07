#pragma once
#include "config.h"

void    ring_init();
bool    ring_push(const WeatherRecord& rec);
bool    ring_latest(WeatherRecord* out);
int     ring_get_all(WeatherRecord* out, int max);
int     ring_count();
