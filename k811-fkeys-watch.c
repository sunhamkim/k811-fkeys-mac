/*
 * k811-fkeys-watch.c
 *
 * Event-driven Logitech K811 configurator for macOS.
 *
 * Karabiner-Elements seizes keyboards exclusively. To coexist with it, set
 * Karabiner's "Delay before opening a device" to a short delay. This watcher
 * observes K811 arrival through the IOKit registry (without opening an
 * IOHIDManager), briefly seizes the device first, applies the desired HID++
 * settings, closes it immediately, and then leaves the device to Karabiner.
 *
 * Current reconnect policy:
 *   - Standard F1-F12 mode (0x40A2 current state = 0)
 *   - Keyboard backlight off (0x1981 = 0)
 *
 * Build:
 *   cc k811-fkeys-watch.c -o k811-fkeys-watch \
 *      -framework IOKit -framework CoreFoundation
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDKeys.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define LOGITECH_VID 0x046D
#define K811_PID     0xB317

/* Give the Bluetooth HID stack time to finish bringing the device up. */
#define RETRY_COUNT     50
#define RETRY_DELAY_US  50000   /* 50 ms; 2.5 s total */

/*
 * HID++ 2.0, K811:
 *
 * Fn inversion:
 *   feature 0x40A2 is index 0x06
 *   function 1, state 0 = standard F1-F12
 *
 * Backlight:
 *   feature 0x1981 is index 0x08
 *   function 1, value 0 = Off
 *
 * Report layout:
 *   report ID       0x10
 *   device index    0xFF (direct-connected Bluetooth device)
 *   function        0x1
 *   software ID     0x4
 */
static const uint8_t standard_fkeys_report[7] = {
    0x10, 0xFF, 0x06, 0x14, 0x00, 0x00, 0x00
};

static const uint8_t backlight_off_report[7] = {
    0x10, 0xFF, 0x08, 0x14, 0x00, 0x00, 0x00
};

static bool apply_k811_settings(IOHIDDeviceRef device)
{
    IOReturn last_open = kIOReturnError;
    IOReturn last_fn_write = kIOReturnError;
    IOReturn last_backlight_write = kIOReturnError;

    for (int attempt = 0; attempt < RETRY_COUNT; ++attempt) {
        /*
         * Seize is intentional. The diagnostic probe succeeds this way;
         * shared-open can succeed while SetReport is still denied. Karabiner's
         * open delay gives us a brief exclusive window here.
         */
        last_open = IOHIDDeviceOpen(device, kIOHIDOptionsTypeSeizeDevice);

        if (last_open == kIOReturnSuccess) {
            last_fn_write = IOHIDDeviceSetReport(
                device,
                kIOHIDReportTypeOutput,
                standard_fkeys_report[0],
                standard_fkeys_report,
                sizeof(standard_fkeys_report));

            last_backlight_write = IOHIDDeviceSetReport(
                device,
                kIOHIDReportTypeOutput,
                backlight_off_report[0],
                backlight_off_report,
                sizeof(backlight_off_report));

            IOHIDDeviceClose(device, kIOHIDOptionsTypeSeizeDevice);

            if (last_fn_write == kIOReturnSuccess &&
                last_backlight_write == kIOReturnSuccess)
                return true;
        }

        usleep(RETRY_DELAY_US);
    }

    fprintf(stderr,
            "K811: failed to apply settings "
            "(open=0x%08x, fn=0x%08x, backlight=0x%08x)\n",
            last_open,
            last_fn_write,
            last_backlight_write);
    return false;
}

/*
 * IOService matching does not open the HID device, so this watcher can remain
 * alive even while Karabiner currently owns the keyboard. When the K811 is
 * power-cycled, a newly matched IOHIDDevice service arrives here.
 */
static void device_matched(void *context, io_iterator_t iterator)
{
    (void)context;

    io_service_t service;

    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        IOHIDDeviceRef device = IOHIDDeviceCreate(kCFAllocatorDefault, service);

        if (device) {
            (void)apply_k811_settings(device);
            CFRelease(device);
        } else {
            fprintf(stderr, "K811: IOHIDDeviceCreate failed.\n");
        }

        IOObjectRelease(service);
    }
}

static CFMutableDictionaryRef make_matching_dictionary(void)
{
    CFMutableDictionaryRef dict = IOServiceMatching("IOHIDDevice");
    if (!dict)
        return NULL;

    int vid = LOGITECH_VID;
    int pid = K811_PID;

    CFNumberRef vid_number = CFNumberCreate(
        kCFAllocatorDefault,
        kCFNumberIntType,
        &vid);

    CFNumberRef pid_number = CFNumberCreate(
        kCFAllocatorDefault,
        kCFNumberIntType,
        &pid);

    if (!vid_number || !pid_number) {
        if (vid_number)
            CFRelease(vid_number);
        if (pid_number)
            CFRelease(pid_number);
        CFRelease(dict);
        return NULL;
    }

    CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey), vid_number);
    CFDictionarySetValue(dict, CFSTR(kIOHIDProductIDKey), pid_number);

    CFRelease(vid_number);
    CFRelease(pid_number);

    return dict;
}

int main(void)
{
    IONotificationPortRef notify_port =
        IONotificationPortCreate(kIOMainPortDefault);

    if (!notify_port) {
        fprintf(stderr, "K811: failed to create IOKit notification port.\n");
        return 1;
    }

    CFRunLoopSourceRef source =
        IONotificationPortGetRunLoopSource(notify_port);

    if (!source) {
        fprintf(stderr, "K811: failed to obtain IOKit run-loop source.\n");
        IONotificationPortDestroy(notify_port);
        return 1;
    }

    CFRunLoopAddSource(
        CFRunLoopGetCurrent(),
        source,
        kCFRunLoopDefaultMode);

    CFMutableDictionaryRef match = make_matching_dictionary();
    if (!match) {
        fprintf(stderr, "K811: failed to create IOKit matching dictionary.\n");
        IONotificationPortDestroy(notify_port);
        return 1;
    }

    io_iterator_t iterator = IO_OBJECT_NULL;

    /*
     * IOServiceAddMatchingNotification consumes the matching dictionary.
     * Draining the iterator once handles an already-present K811 and arms the
     * notification for future arrivals.
     */
    kern_return_t kr = IOServiceAddMatchingNotification(
        notify_port,
        kIOFirstMatchNotification,
        match,
        device_matched,
        NULL,
        &iterator);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr,
                "K811: IOServiceAddMatchingNotification failed: 0x%08x\n",
                kr);
        IONotificationPortDestroy(notify_port);
        return 1;
    }

    device_matched(NULL, iterator);

    CFRunLoopRun();

    if (iterator != IO_OBJECT_NULL)
        IOObjectRelease(iterator);

    CFRunLoopRemoveSource(
        CFRunLoopGetCurrent(),
        source,
        kCFRunLoopDefaultMode);

    IONotificationPortDestroy(notify_port);
    return 0;
}
