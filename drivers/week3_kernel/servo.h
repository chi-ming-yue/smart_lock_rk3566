#ifndef _SERVO_H
#define _SERVO_H

int servo_init(void);
void servo_deinit(void);
int servo_start_sweep(void);
void servo_stop_sweep(void);
int servo_move_center(void);
int servo_move_left(void);
int servo_move_right(void);
int servo_move_lock(void);
int servo_move_unlock(void);

#endif
