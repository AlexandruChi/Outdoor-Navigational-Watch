#include "SystemStats.h"

#if PRINT_STATS == 1
#include <zephyr/kernel.h>

#if LOG_HEAP == 1
extern struct sys_heap _system_heap;

void print_heap(struct k_work *work) {
    struct sys_memory_stats stats;
    if (!sys_heap_runtime_stats_get(&_system_heap, &stats)) {
        printk("Heap: %zu/%zu bytes used (Max: %zu)\n\n", stats.allocated_bytes, stats.free_bytes + stats.allocated_bytes, stats.max_allocated_bytes);
    }
}

K_WORK_DEFINE(print_heap_work, print_heap);

void log_heap(struct k_timer *timer_id) {
    k_work_submit(&print_heap_work);
}

K_TIMER_DEFINE(log_heap_timer, log_heap, NULL);

#endif /* LOG_HEAP */

#if LOG_DEADLINE == 1
extern atomic_t segmentDisplayMissCount;
extern atomic_t altimeterMissCount;

atomic_t segmentDisplayMisses;
atomic_t altimeterMisses;

void print_deadline(struct k_work *work) {
    int64_t segmentDisplayMissesVal = atomic_get(&segmentDisplayMisses);
    int64_t altimeterMissesVal = atomic_get(&altimeterMisses);

    printk("Deadline misses in last 10 seconds: \n");
    printk(" segment display: %lld\n", segmentDisplayMissesVal);
    printk(" altimeter: %lld\n", altimeterMissesVal);
    printk("\n");
}

K_WORK_DEFINE(deadline_work, print_deadline);

void log_deadline(struct k_timer *timer_id) {
    atomic_set(&segmentDisplayMisses, atomic_get(&segmentDisplayMissCount));
    atomic_set(&altimeterMisses, atomic_get(&altimeterMissCount));

    atomic_set(&segmentDisplayMissCount, 0);
    atomic_set(&altimeterMissCount, 0);

    k_work_submit(&deadline_work);
}

K_TIMER_DEFINE(log_deadline_timer, log_deadline, NULL);

#endif /* LOG_DEADLINE */

#if LOG_LATENCY == 1
extern atomic_t segmentDisplayLatencyUs;
extern atomic_t altimeterLatencyUs;

atomic_t segmentDisplayMaxLatencyUs;
atomic_t altimeterMaxLatencyUs;

void print_latency(struct k_work *work) {
    int64_t segmentDisplayMaxLatencyUsVal = atomic_get(&segmentDisplayMaxLatencyUs);
    int64_t altimeterMaxLatencyUsVal = atomic_get(&altimeterMaxLatencyUs);

    printk("Max latency in last 10 seconds: \n");
    printk(" segment display: %lld us\n", segmentDisplayMaxLatencyUsVal);
    printk(" altimeter: %lld us\n", altimeterMaxLatencyUsVal);
    printk("\n");
}

K_WORK_DEFINE(latency_work, print_latency);

void log_latency(struct k_timer *timer_id) {
    atomic_set(&segmentDisplayMaxLatencyUs, atomic_get(&segmentDisplayLatencyUs));
    atomic_set(&altimeterMaxLatencyUs, atomic_get(&altimeterLatencyUs));

    atomic_set(&segmentDisplayLatencyUs, 0);
    atomic_set(&altimeterLatencyUs, 0);

    k_work_submit(&latency_work);
}

K_TIMER_DEFINE(log_latency_timer, log_latency, NULL);

#endif /* LOG_LATENCY */

#if LOG_JITTER == 1
extern atomic_t segmentDisplayJitterUs;
extern atomic_t altimeterJitterUs;

void print_jitter(struct k_work *work) {
    int64_t segmentDisplayJitterUsVal = atomic_get(&segmentDisplayJitterUs);
    int64_t altimeterJitterUsVal = atomic_get(&altimeterJitterUs);

    printk("Jitter: \n");
    printk(" segment display: %lld us\n", segmentDisplayJitterUsVal);
    printk(" altimeter: %lld us\n", altimeterJitterUsVal);
    printk("\n");
}

K_WORK_DEFINE(jitter_work, print_jitter);

void log_jitter(struct k_timer *timer_id) {
    k_work_submit(&jitter_work);
}

K_TIMER_DEFINE(log_jitter_timer, log_jitter, NULL);

#endif /* LOG_JITTER */

#if LOG_MEMORY == 1
#include <zephyr/debug/thread_analyzer.h>

void print_memory(struct k_work *work) {
    thread_analyzer_print(0);
    printk("\n");
}

K_WORK_DEFINE(memory_work, print_memory);

void log_memory(struct k_timer *timer_id) {
    k_work_submit(&memory_work);
}

K_TIMER_DEFINE(log_memory_timer, log_memory, NULL);

#endif /* LOG_MEMORY */

int main(void) {
    printk("Tick Rate: %d Hz\n\n", CONFIG_SYS_CLOCK_TICKS_PER_SEC);

    #if LOG_HEAP == 1
    k_timer_start(&log_heap_timer, K_MINUTES(1), K_MINUTES(1));
    #endif /* LOG_HEAP */

    #if LOG_MEMORY == 1
    k_timer_start(&log_memory_timer, K_MINUTES(1), K_MINUTES(1));
    #endif /* LOG_MEMORY */

    #if LOG_JITTER == 1
    k_timer_start(&log_jitter_timer, K_MINUTES(1), K_MINUTES(1));
    #endif /* LOG_JITTER */

    #if LOG_DEADLINE == 1
    k_timer_start(&log_deadline_timer, K_SECONDS(10), K_SECONDS(10));
    #endif /* LOG_DEADLINE */

    #if LOG_LATENCY == 1
    k_timer_start(&log_latency_timer, K_SECONDS(10), K_SECONDS(10));
    #endif /* LOG_LATENCY */

    return 0;
}

#else /* PRINT_STATS */

int main(void) {
    return 0;
}

#endif /* PRINT_STATS */
