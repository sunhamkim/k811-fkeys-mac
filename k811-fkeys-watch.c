/*
 * k811-fkeys-watch.c
 *
 * Event-driven Logitech K811 Fn-key configurator for macOS.
 *
 * The K811 forgets its Fn-inversion state when powered off. This daemon
 * watches for K811 HID arrival and, before Karabiner-Elements seizes the
 * device, sets HID++ 0x40A2 current state to 0 (standard F1-F12).
 *
 * Karabiner-Elements must have a sufficiently long
 * delay_milliseconds_before_open_device so this program can open the K811
 * first. Start with 5000 ms; once stable, reduce it experimentally.
 *
 * Build:
 *   cc k811-fkeys-watch.c -o k811-fkeys-watch \
 *      -framework IOKit -framework CoreFoundation
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define LOGITECH_VID 0x046D
#define K811_PID     0xB317

#define RETRY_COUNT     20
#define RETRY_DELAY_US  100000  /* 100 ms; 2 s total retry window */

/*
 * HID++ 2.0, K811:
 *   report ID       0x10
 *   device index    0xFF (direct-connected Bluetooth device)
 *   feature index   0x06 (0x40A2 Fn Inversion with Default State)
 *   function        0x1
 *   software ID     0x4
 *   state           0x00 (standard F1-F12)
 */
static const uint8_t standard_fkeys_report[7] = {
    0x10, 0xFF, 0x06, 0x14, 0x00, 0x00, 0x00
};

static bool set_standard_fkeys(IOHIDDeviceRef device)
{
    IOReturn last_open = kIOReturnError;
    IOReturn last_write = kIOReturnError;

    for (int attempt = 0; attempt < RETRY_COUNT; ++attempt) {
        last_open = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);

        if (last_open == kIOReturnSuccess) {
            last_write = IOHIDDeviceSetReport(
                device,
                kIOHIDReportTypeOutput,
                standard_fkeys_report[0],
                standard_fkeys_report,
                sizeof(standard_fkeys_report));

            IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);

            if (last_write == kIOReturnSuccess)
                return true;
        }

        usleep(RETRY_DELAY_US);
    }

    fprintf(stderr,
            "K811: failed to set F-key mode (open=0x%08x, write=0x%08x)\n",
            last_open,
            last_write);
    return false;
}

static void device_matched(void *context,
                           IOReturn result,
                           void *sender,
                           IOHIDDeviceRef device)
{
    (void)context;
    (void)sender;

    if (result != kIOReturnSuccess)
        return;

    (void)set_standard_fkeys(device);
}

static CFMutableDictionaryRef make_matching_dictionary(void)
{
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

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
    IOHIDManagerRef manager = IOHIDManagerCreate(
        kCFAllocatorDefault,
        kIOHIDOptionsTypeNone);

    if (!manager) {
        fprintf(stderr, "K811: failed to create IOHIDManager.\n");
        return 1;
    }

    CFMutableDictionaryRef match = make_matching_dictionary();
    if (!match) {
        fprintf(stderr, "K811: failed to create HID matching dictionary.\n");
        CFRelease(manager);
        return 1;
    }

    IOHIDManagerSetDeviceMatching(manager, match);
    IOHIDManagerRegisterDeviceMatchingCallback(manager, device_matched, NULL);
    IOHIDManagerScheduleWithRunLoop(
        manager,
        CFRunLoopGetCurrent(),
        kCFRunLoopDefaultMode);

    IOReturn ret = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (ret != kIOReturnSuccess) {
        fprintf(stderr, "K811: IOHIDManagerOpen failed: 0x%08x\n", ret);
        CFRelease(match);
        CFRelease(manager);
        return 1;
    }

    CFRelease(match);

    /* Existing matching devices are delivered through the matching callback;
       future reconnects are delivered the same way. */
    CFRunLoopRun();

    IOHIDManagerUnscheduleFromRunLoop(
        manager,
        CFRunLoopGetCurrent(),
        kCFRunLoopDefaultMode);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);

    return 0;
}
