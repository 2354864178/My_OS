#ifndef ONIX_RTC_H
#define ONIX_RTC_H

#include <onix/types.h>

// CMOS I/O ports
#define CMOS_ADDR_PORT 0x70
#define CMOS_DATA_PORT 0x71

// Time registers (current time)
#define CMOS_REG_SECONDS 0x00
#define CMOS_REG_MINUTES 0x02
#define CMOS_REG_HOURS   0x04
#define CMOS_REG_WEEKDAY 0x06
#define CMOS_REG_DAY     0x07
#define CMOS_REG_MONTH   0x08
#define CMOS_REG_YEAR    0x09
#define CMOS_REG_CENTURY 0x32

// Alarm registers
#define CMOS_REG_SECONDS_ALARM 0x01
#define CMOS_REG_MINUTES_ALARM 0x03
#define CMOS_REG_HOURS_ALARM   0x05

// Control/status registers
#define CMOS_REG_A 0x0a
#define CMOS_REG_B 0x0b
#define CMOS_REG_C 0x0c
#define CMOS_REG_D 0x0d

#define CMOS_NMI_MASK 0x80

typedef struct rtc_dt_info
{
	bool present;     // 是否从设备树读取到 CMOS 信息
	u32 addr_port;    // CMOS 地址端口
	u32 data_port;    // CMOS 数据端口
	u32 irq;          // RTC 中断号
} rtc_dt_info_t;

void set_alarm(u32 secs);
u8 cmos_read(u8 addr);
void cmos_write(u8 addr, u8 value);
const rtc_dt_info_t *rtc_dt_get(void);

#endif