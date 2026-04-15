#include <zephyr/kernel.h>

int main(void) {
    printk("Tick Rate: %d Hz\n", CONFIG_SYS_CLOCK_TICKS_PER_SEC);
	return 0;
}