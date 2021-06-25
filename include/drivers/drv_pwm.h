#ifndef __DRV_PWM_H__
#define __DRV_PWM_H__

#include <stdbool.h>
#include <stdint.h>
#include "nrf.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "app_pwm.h"

void drv_pwm_init(void);
void drv_pwm_test(void);
void drv_pwm_out(uint8_t value);





#endif
                                                                     