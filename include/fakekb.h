#ifndef FAKEKB_H
#define FAKEKB_H

#ifdef __cplusplus
extern "C" {
#endif

int fakekb_init();
void fakekb_quit();

void fakekb_pump_events();

#ifdef __cplusplus
}
#endif

#endif
