#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <stdio.h>
#include "keypad.h"

static void displayTask(void);

K_THREAD_DEFINE(displayThread, 1024, displayTask, NULL, NULL, NULL, 5, 0, 0);

struct page {
    char title[13];
    void (*update)(struct page *page);
    void (*action)(struct page *page);
    void (*render)(struct page *page);
    void *pageData;
};

void updateTimePage(struct page *page);
void renderTimePage(struct page *page);

struct page pages[7] = {
    {
        .title = "Time",
    }, {
        .title = "Position",
    }, {
        .title = "Altitude",
    }, {
        .title = "Environment",
    }, {
        .title = "Magnetometer",
    }, {
        .title = "GNSS",
    }, {
        .title = "System",
    }
};


static void displayTask(void) {
    k_thread_name_set(displayThread, "display");

    const struct device *display = DEVICE_DT_GET(DT_ALIAS(display));

    __ASSERT(device_is_ready(display), "Display not ready!");

    if (cfb_framebuffer_init(display)) {
        printf("CFB initialization failed!\n");
    }

    if (cfb_framebuffer_clear(display, true)) {
        printf("Failed to clear framebuffer\n");
    }

    if (cfb_framebuffer_set_font(display, 0)) {
        printf("Failed to set font\n");
    }

    display_blanking_on(display);

    struct page *currentPage = &pages[0];
    bool displayOn = false;

    while (1) {
        uint32_t events = k_event_wait(&navigationEvent, (KEY_EVT_NAVIGATION_POWER | KEY_EVT_NAVIGATION_LEFT | KEY_EVT_NAVIGATION_RIGHT | KEY_EVT_NAVIGATION_SELECT), true, K_SECONDS(1));

        if (displayOn == false) {
            if (events & KEY_EVT_NAVIGATION_POWER) {
                display_blanking_off(display);
                displayOn = true;
            } else {
                continue;
            }
        } else if (events) {
            if (events & KEY_EVT_NAVIGATION_POWER) {
                cfb_framebuffer_clear(display, true);
                display_blanking_on(display);
                displayOn = false;
                continue;
            }
            
            if (events & KEY_EVT_NAVIGATION_LEFT) {
                int currentPageIndex = currentPage - pages;
                currentPageIndex = (currentPageIndex - 1 + ARRAY_SIZE(pages)) % ARRAY_SIZE(pages);
                currentPage = &pages[currentPageIndex];
            } else if (events & KEY_EVT_NAVIGATION_RIGHT) {
                int currentPageIndex = currentPage - pages;
                currentPageIndex = (currentPageIndex + 1) % ARRAY_SIZE(pages);
                currentPage = &pages[currentPageIndex];
            } else if (events & KEY_EVT_NAVIGATION_SELECT) {
                if (currentPage->action) {
                    currentPage->action(currentPage);
                }
            }
        }

        cfb_framebuffer_clear(display, false);
        if (currentPage) {
            cfb_draw_text(display, currentPage->title, 0, 0);

            if (currentPage->update) {
                currentPage->update(currentPage);
            }

            if (currentPage->render) {
                currentPage->render(currentPage);
            }

            cfb_framebuffer_finalize(display);
        }
    }
}
