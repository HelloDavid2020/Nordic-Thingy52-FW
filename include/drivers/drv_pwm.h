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
void drv_pwm_duty_out(uint8_t value);

void drv_pwm_ticks_out(uint32_t value);
void drv_pwm_lock(void);
void drv_pwm_unlock(void);




#endif

