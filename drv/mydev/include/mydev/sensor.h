#ifndef _SENSOR_H
#define _SENSOR_H

extern char sensor_state;
extern int irq_number;

int hc_sr501_init(void);
char hc_sr501_refresh_state(void);
void hc_sr501_deinit(void);

#endif
