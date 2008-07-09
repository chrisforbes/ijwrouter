#ifndef BILLING_H
#define BILLING_H

u32 get_start_of_period(void);
u32 get_end_of_period(void);
void set_rollover_day(u08 day);
u08 get_rollover_day(void);

#endif
