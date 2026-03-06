#include "os.h"

extern uint32_t _bss_start, _bss_end;
extern uint32_t _vecbase_start;

#define RTC_CNTL_WDTWPROTECT_REG (*((volatile uint32_t *)0x3FF480A4))
#define RTC_CNTL_WDTCONFIG0_REG (*((volatile uint32_t *)0x3FF4808C))
#define TIMERG0_WDTWPROTECT_REG  (*((volatile uint32_t *)0x3FF5F064))
#define TIMERG0_WDTCONFIG0_REG   (*((volatile uint32_t *)0x3FF5F048))
#define TIMERG1_WDTWPROTECT_REG  (*((volatile uint32_t *)0x3FF60064))
#define TIMERG1_WDTCONFIG0_REG   (*((volatile uint32_t *)0x3FF60048))

#define UART0_FIFO_REG (*((volatile uint32_t *)0x3FF40000))
#define UART0_STATUS_REG (*((volatile uint32_t *)0x3FF4001C))
#define GPIO_ENABLE_REG (*((volatile uint32_t *)0x3FF44020))
#define GPIO_OUT_REG (*((volatile uint32_t *)0x3FF44004))
#define ONBOARD_LED_PIN 2

static inline __attribute__((always_inline)) void uart_putc(char c) {
	while (((UART0_STATUS_REG >> 16) & 0xFF) >= 127)
		;
	UART0_FIFO_REG = c;
}
static inline __attribute__((always_inline)) void uart_puts(const char *str) {
	while (*str)
		uart_putc(*str++);
}

static inline __attribute__((always_inline)) void uart_print_uint(uint32_t val) {
    char buffer[11]; int i = 9; buffer[10] = '\0';
    if (val == 0) { uart_putc('0'); return; }
    while (val > 0) { buffer[i--] = (val % 10) + '0'; val /= 10; }
    uart_puts(&buffer[i + 1]);
}

uint8_t stack_low[SRTOS_STACK_SIZE] __attribute__((aligned(16)));
uint8_t stack_med[SRTOS_STACK_SIZE] __attribute__((aligned(16)));
uint8_t stack_high[SRTOS_STACK_SIZE] __attribute__((aligned(16)));
uint8_t stack_ultra[SRTOS_STACK_SIZE] __attribute__((aligned(16)));
uint8_t stack_heartbeat[SRTOS_STACK_SIZE] __attribute__((aligned(16)));
uint8_t stack_idle[SRTOS_STACK_SIZE] __attribute__((aligned(16)));

srtos_mutex_t uart_mutex;
srtos_mutex_t mutex_A;
srtos_mutex_t mutex_B;

void srtos_idle_task() { 
    while(1) { 
        for(volatile int i = 0; i < 2000000; i++); 
        
        uart_putc('.'); 
    } 
}

void telemetry_task() { 
    while(1) { 
        srtos_delay_ms(500);
        
        srtos_mutex_take(&uart_mutex);
        uart_puts("\n[TEL] Tick: "); uart_print_uint(system_ticks); 
        uart_puts(" | L_St: "); uart_print_uint(task_list[0].state); uart_puts(" L_Pri: "); uart_print_uint(task_list[0].priority);
        uart_puts(" | M_St: "); uart_print_uint(task_list[1].state); uart_puts(" M_Pri: "); uart_print_uint(task_list[1].priority);
        uart_puts(" | H_St: "); uart_print_uint(task_list[2].state); uart_puts(" H_Pri: "); uart_print_uint(task_list[2].priority);
        uart_puts(" | U_St: "); uart_print_uint(task_list[3].state); uart_puts(" U_Pri: "); uart_print_uint(task_list[3].priority);
        uart_puts("\r\n");
        srtos_mutex_give(&uart_mutex);
        
        GPIO_OUT_REG ^= (1 << ONBOARD_LED_PIN); // LED toggle
    } 
}

uint32_t sensor_buffer[5];
srtos_queue_t sensor_queue;

void producer_task() { 
    uint32_t mock_accel_z = 981; 
    
    while(1) {
        srtos_mutex_take(&uart_mutex);
        uart_puts("[Producer] Uretiyor: "); uart_print_uint(mock_accel_z); 
        uart_puts("\r\n");
        srtos_mutex_give(&uart_mutex);

        srtos_queue_send_blocking(&sensor_queue, mock_accel_z);
        
        srtos_mutex_take(&uart_mutex);
        uart_puts("[Producer] >>> Kuyruga eklendi! Icerideki Adet: "); 
        uart_print_uint(sensor_queue.count); uart_puts("\r\n");
        srtos_mutex_give(&uart_mutex);
        
        mock_accel_z += 5; 
        
        srtos_delay_ms(100); 
    }
}

void consumer_task() { 
    uint32_t received_data;
    
    while(1) {
        srtos_delay_ms(2000); 
        
        srtos_queue_receive_blocking(&sensor_queue, &received_data);

        srtos_mutex_take(&uart_mutex);
        uart_puts("\n*** [Consumer] BIR VERI ERITTI: "); uart_print_uint(received_data); 
        uart_puts(" (Kalan Adet: "); uart_print_uint(sensor_queue.count); 
        uart_puts(") ***\r\n\n");
        srtos_mutex_give(&uart_mutex);
    }
}

void os_main() {
    for(int i = 0; i < SRTOS_MAX_TASKS; i++) {
        task_list[i].state = SRTOS_STATE_BLOCKED;
        task_list[i].wake_tick = 0xFFFFFFFF;
        task_list[i].priority = -1;
    }

    RTC_CNTL_WDTWPROTECT_REG = 0x50D83AA1;
    RTC_CNTL_WDTCONFIG0_REG = 0;
    
    TIMERG0_WDTWPROTECT_REG = 0x50D83AA1;
    TIMERG0_WDTCONFIG0_REG = 0;
    TIMERG1_WDTWPROTECT_REG = 0x50D83AA1;
    TIMERG1_WDTCONFIG0_REG = 0;

    GPIO_ENABLE_REG |= (1 << ONBOARD_LED_PIN);

    srtos_mutex_init(&uart_mutex);
    
    srtos_queue_init(&sensor_queue, sensor_buffer, 5);

    __asm__ __volatile__("wsr %0, vecbase \n isync" ::"r"(&_vecbase_start));
    __asm__ __volatile__("wsr %0, intclear \n isync" ::"r"(0xFFFFFFFF));
    uint32_t ccount;
    __asm__ __volatile__("rsr %0, ccount" : "=r"(ccount));
    __asm__ __volatile__("wsr %0, ccompare0 \n isync" ::"r"(ccount + 80000));

    uart_puts("\r\n--- SinceRTOS MESSAGE QUEUE TEST ---\r\n");

    srtos_create_task(0, 1, producer_task, stack_low);
    srtos_create_task(1, 2, consumer_task, stack_med);
    srtos_create_task(4, 5, telemetry_task, stack_heartbeat);
    srtos_create_task(5, 0, srtos_idle_task, stack_idle);

    __asm__ __volatile__("wsr %0, intenable \n isync" ::"r"((1 << 6) | (1 << 7)));
    uint32_t ps_val;
    __asm__ __volatile__("rsr %0, ps" : "=r"(ps_val));
    ps_val &= ~(0x1F);
    __asm__ __volatile__("wsr %0, ps \n isync" ::"r"(ps_val));

    current_task = 0;
    producer_task();
}

void call_start_cpu0() {
	uint32_t *bss_ptr = &_bss_start;
	while (bss_ptr < &_bss_end) {
		*bss_ptr = 0;
		bss_ptr++;
	}
	os_main();
}
