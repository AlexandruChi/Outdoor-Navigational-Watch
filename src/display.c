#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <stdio.h>
#include "keypad.h"
#include "data.h"

#include "gnss.h"
#include "altimeter.h"

static void displayTask(void);

#define DISPLAY_UPDATE_INTERVAL K_MSEC(100)

K_THREAD_DEFINE(displayThread, 1024, displayTask, NULL, NULL, NULL, 5, 0, 0);

struct page {
    char title[13];
    void (*action)(struct page *page);
    void (*render)(struct page *page);
    uint8_t state;
};

void renderTimePage(struct page *page);
void renderDatePage(struct page *page);
void renderPositionPage(struct page *page);
void actionPositionPage(struct page *page);
void renderAltitudePage(struct page *page);
void actionAltitudePage(struct page *page);
void renderEnvironmentPage(struct page *page);
void actionEnvironmentPage(struct page *page);
void renderDirectionPage(struct page *page);
void renderGnssPage(struct page *page);

struct page pages[7] = {
    {
        .title = "Time UTC",
        .render = renderTimePage,
    }, {
        .title = "Date",
        .render = renderDatePage,
    }, {
        .title = "Position",
        .render = renderPositionPage,
        .action = actionPositionPage,
    }, {
        .title = "Altitude",
        .render = renderAltitudePage,
        .action = actionAltitudePage,
    }, {
        .title = "Environment",
        .render = renderEnvironmentPage,
        .action = actionEnvironmentPage,
    }, {
        .title = "Direction",
        .render = renderDirectionPage,
    }, {
        .title = "GNSS status",
        .render = renderGnssPage,
    }
};

void fetchData();

static void displayTask(void) {
    k_thread_name_set(displayThread, "display");

    const struct device *display = DEVICE_DT_GET(DT_ALIAS(display));

    if (!device_is_ready(display)) {
        printk("Display not ready!\n");
        k_panic();
    }

    if (cfb_framebuffer_init(display)) {
        printk("CFB initialization failed!\n");
        k_panic();
    }

    if (cfb_framebuffer_clear(display, true)) {
        printk("Failed to clear framebuffer\n");
        k_panic();
    }

    if (cfb_framebuffer_set_font(display, 0)) {
        printk("Failed to set font\n");
        k_panic();
    }

    display_blanking_on(display);

    struct page *currentPage = &pages[0];
    bool displayOn = false;

    while (1) {
        uint32_t events = k_event_wait(&navigationEvent, (KEY_EVT_NAVIGATION_POWER | KEY_EVT_NAVIGATION_LEFT | KEY_EVT_NAVIGATION_RIGHT | KEY_EVT_NAVIGATION_SELECT), false, DISPLAY_UPDATE_INTERVAL);
        k_event_set(&navigationEvent, 0);

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
            
            if (events & KEY_EVT_NAVIGATION_RIGHT) {
                int currentPageIndex = currentPage - pages;
                currentPageIndex = (currentPageIndex + 1) % ARRAY_SIZE(pages);
                currentPage = &pages[currentPageIndex];
            } else if (events & KEY_EVT_NAVIGATION_LEFT) {
                int currentPageIndex = currentPage - pages;
                currentPageIndex = (currentPageIndex - 1 + ARRAY_SIZE(pages)) % ARRAY_SIZE(pages);
                currentPage = &pages[currentPageIndex];
            } else if (events & KEY_EVT_NAVIGATION_SELECT) {
                if (currentPage->action) {
                    currentPage->action(currentPage);
                }
            }
        }

        fetchData();

        cfb_framebuffer_clear(display, false);
        if (currentPage) {
            cfb_draw_text(display, currentPage->title, 0, 0);

            if (currentPage->render) {
                currentPage->render(currentPage);
            }

            cfb_framebuffer_finalize(display);
        }
    }
}

static gnss_data_t gnssData;
static altimeter_data_t altimeterData;

void fetchData() {
    gnssGetData(&gnssData, K_NO_WAIT);
    altimeterGetData(&altimeterData, K_NO_WAIT);
}

void renderTimePage(struct page *page) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", gnssData.datetime.time.hour, gnssData.datetime.time.minute, gnssData.datetime.time.second);
    cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 30);
}

void renderDatePage(struct page *page) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%02d", gnssData.datetime.date.day, gnssData.datetime.date.month, gnssData.datetime.date.year);
    cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 30);
}

void renderPositionPage(struct page *page) {
    char buffer[32];

    if (page->state) {
        snprintf(buffer, sizeof(buffer), "Lt %8.3f", (double)gnssData.position.latitude);
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 5, 23);
        snprintf(buffer, sizeof(buffer), "Ln %8.3f", (double)gnssData.position.longitude);
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 5, 37);
    } else {
        uint8_t hLat = gnssData.position.latitude >= 0 ? 'N' : 'S';
        uint8_t hLon = gnssData.position.longitude >= 0 ? 'E' : 'W';

        float absLat = gnssData.position.latitude >= 0 ? gnssData.position.latitude : -gnssData.position.latitude;
        float absLon = gnssData.position.longitude >= 0 ? gnssData.position.longitude : -gnssData.position.longitude;

        uint8_t latDeg = (uint8_t)absLat;
        uint8_t latMin = (uint8_t)((absLat - latDeg) * 60);
        uint8_t latSec = (uint8_t)(((absLat - latDeg) * 60 - latMin) * 60);

        uint8_t lonDeg = (uint8_t)absLon;
        uint8_t lonMin = (uint8_t)((absLon - lonDeg) * 60);
        uint8_t lonSec = (uint8_t)(((absLon - lonDeg) * 60 - lonMin) * 60);

        snprintf(buffer, sizeof(buffer), "   o");
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 5, 18);
        snprintf(buffer, sizeof(buffer), "%3d %02d'%02d\" %c", latDeg, latMin, latSec, hLat);
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 5, 23);
        snprintf(buffer, sizeof(buffer), "   o");
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 5, 32);
        snprintf(buffer, sizeof(buffer), "%3d %02d'%02d\" %c", lonDeg, lonMin, lonSec, hLon);
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 5, 37);
    }
}

void actionPositionPage(struct page *page) {
    page->state = !page->state;   
}

void renderAltitudePage(struct page *page) {
    char buffer[32];

    switch (page->state) {
        case 0:
            snprintf(buffer, sizeof(buffer), "%6d m", altimeterData.altitude);
            cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 30);
            break;
        case 1:
            snprintf(buffer, sizeof(buffer), "GNSS");
            cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 25, 17);
            snprintf(buffer, sizeof(buffer), "%6d m", gnssData.altitude);
            cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 30);
            break;
        case 2:
            snprintf(buffer, sizeof(buffer), "Adjusted");
            cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 25, 17);
            snprintf(buffer, sizeof(buffer), "%6d m", altimeterData.adjAltitude);
            cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 30);
            break;
    }
}

void actionAltitudePage(struct page *page) {
    page->state++;
    if (page->state > 2) {
        page->state = 0;
    }
}

void renderEnvironmentPage(struct page *page) {
    char buffer[32];

    if (page->state) {
        snprintf(buffer, sizeof(buffer), "%7.2f K", (double)(altimeterData.environment.temperature + 273.15f));
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 17);
        snprintf(buffer, sizeof(buffer), "%7.2f kPa", (double)(altimeterData.environment.pressure / 1000.0f));
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 30);
    } else {
        snprintf(buffer, sizeof(buffer), "        o");
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 12);
        snprintf(buffer, sizeof(buffer), "%7.2f  C", (double)altimeterData.environment.temperature);
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 17);
        snprintf(buffer, sizeof(buffer), "%7.2f atm", (double)(altimeterData.environment.pressure / 10132.5f));
        cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 30);
    }

    snprintf(buffer, sizeof(buffer), "%7.2f %%", (double)altimeterData.environment.humidity);
    cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 10, 43);
}

void actionEnvironmentPage(struct page *page) {
    page->state = !page->state;
}

void renderDirectionPage(struct page *page) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "     o");
    cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 25, 18);
    snprintf(buffer, sizeof(buffer), "%5d", gnssData.heading);
    cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 25, 23);
    snprintf(buffer, sizeof(buffer), "%5.1f m/s", (double)gnssData.speed);
    cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 25, 37);
}

void renderGnssPage(struct page *page) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Nr. sat: %d", gnssData.gnss.satellites);
    cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 5, 43);
    snprintf(buffer, sizeof(buffer), "%s", gnssData.gnss.quality);
    cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 5, 17);
    snprintf(buffer, sizeof(buffer), "%s", gnssData.gnss.fixStatus);
    cfb_draw_text(DEVICE_DT_GET(DT_ALIAS(display)), buffer, 5, 30);
}
