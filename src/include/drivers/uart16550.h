#ifndef __DRIVERS_UART16550_H__
#define __DRIVERS_UART16550_H__

int uart_init(unsigned long base, unsigned long clock_freq, unsigned int baud, bool console);

#endif /* __DRIVERS_UART16550_H__ */
