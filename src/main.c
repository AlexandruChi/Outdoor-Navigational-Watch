#include <zephyr/kernel.h>
#include <zephyr/debug/thread_analyzer.h>

extern struct sys_heap _system_heap;

extern atomic_t segmentDisplayMissCount;
extern atomic_t segmentDisplayLatencyUs;
extern atomic_t segmentDisplayJitterUs;

extern atomic_t altimeterMissCount;
extern atomic_t altimeterLatencyUs;
extern atomic_t altimeterJitterUs;

void log_heap(struct k_timer *timer_id) {
    struct sys_memory_stats stats;
    if (!sys_heap_runtime_stats_get(&_system_heap, &stats)) {
        printk("Heap: %zu/%zu bytes used (Max: %zu)\n\n", stats.allocated_bytes, stats.free_bytes + stats.allocated_bytes, stats.max_allocated_bytes);
    }
}

void log_memory(struct k_timer *timer_id) {
    thread_analyzer_print(0);
    printk("\n");
}

void log_deadline(struct k_timer *timer_id) {
    int64_t segmentDisplayMisses = atomic_get(&segmentDisplayMissCount);
    int64_t altimeterMisses = atomic_get(&altimeterMissCount);

    atomic_set(&segmentDisplayMissCount, 0);
    atomic_set(&altimeterMissCount, 0);

    printk("Deadline misses in last 10 seconds: \n");
    printk(" segment display: %lld\n", segmentDisplayMisses);
    printk(" altimeter: %lld\n", altimeterMisses);
    printk("\n");
}

void log_jitter(struct k_timer *timer_id) {
    int64_t segmentDisplayJitterUsVal = atomic_get(&segmentDisplayJitterUs);
    int64_t altimeterJitterUsVal = atomic_get(&altimeterJitterUs);

    printk("Jitter: \n");
    printk(" segment display: %lld us\n", segmentDisplayJitterUsVal);
    printk(" altimeter: %lld us\n", altimeterJitterUsVal);
    printk("\n");
}

void log_latency(struct k_timer *timer_id) {
    int64_t segmentDisplayLatencyUsVal = atomic_get(&segmentDisplayLatencyUs);
    int64_t altimeterLatencyUsVal = atomic_get(&altimeterLatencyUs);

    atomic_set(&segmentDisplayLatencyUs, 0);
    atomic_set(&altimeterLatencyUs, 0);

    printk("Max latency in last 10 seconds: \n");
    printk(" segment display: %lld us\n", segmentDisplayLatencyUsVal);
    printk(" altimeter: %lld us\n", altimeterLatencyUsVal);
    printk("\n");
}

K_TIMER_DEFINE(log_heap_timer, log_heap, NULL);
K_TIMER_DEFINE(log_memory_timer, log_memory, NULL);
K_TIMER_DEFINE(log_deadline_timer, log_deadline, NULL);
K_TIMER_DEFINE(log_jitter_timer, log_jitter, NULL);
K_TIMER_DEFINE(log_latency_timer, log_latency, NULL);

int main(void) {
    printk("Tick Rate: %d Hz\n\n", CONFIG_SYS_CLOCK_TICKS_PER_SEC);

    k_timer_start(&log_heap_timer, K_MINUTES(1), K_MINUTES(1));
    k_timer_start(&log_memory_timer, K_MINUTES(1), K_MINUTES(1));
    k_timer_start(&log_jitter_timer, K_MINUTES(1), K_MINUTES(1));
    k_timer_start(&log_deadline_timer, K_SECONDS(10), K_SECONDS(10));
    k_timer_start(&log_latency_timer, K_SECONDS(10), K_SECONDS(10));

    return 0;
}
