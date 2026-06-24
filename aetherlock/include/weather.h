#ifndef _WEATHER_H
#define _WEATHER_H

#include <stdbool.h>

struct weather_data {
    char location[64];
    char condition[64];
    char temperature[64];
};

struct aetherlock_state;

void weather_init(struct aetherlock_state *state);
void weather_fetch(struct aetherlock_state *state);

#endif
