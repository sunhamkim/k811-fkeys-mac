/*
 * k811-backlight-probe.c
 *
 * Diagnostic probe for the Logitech K811 HID++ 0x1981 BACKLIGHT feature.
 * It does not install anything and does not modify the watcher.
 *
 * The K811 exposes BACKLIGHT (0x1981) at feature index 0x08.  This program:
 *   1. reads the current value (function 0), then
 *   2. asks the keyboard to set value 0 (function 1), and
 *   3. prints the actual HID++ replies, including HID++ error replies.
 *
 * Build:
 *   cc k811-backlight-probe.c -o k811-backlight-probe \
 *      -framework IOKit -framework CoreFoundation
 *
 * Usage (quit/disable Karabiner-Elements first so it does not own the K811):
 *   sudo ./k811-backlight-probe
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGITECH_VID 0x046D
#define K811_PID     0xB317
#define BACKLIGHT_FEATURE_INDEX 0x08

static uint8_t input_buffer[64];
static volatile bool waiting_for_reply = false;
static volatile bool got_reply = false;
static uint8_t expected_function_swid = 0;

static void print_hex(const char *prefix, const uint8_t *data, CFIndex len)
{
    printf("%s", prefix);
    for (CFIndex i = 0; i < len; ++i)
        printf("%s%02X", i ? " " : "", data[i]);
    printf("\n");
}

static const char *hidpp20_error_name(uint8_t code)
{
    switch (code) {
        case 0x00: return "no error";
        case 0x01: return "unknown";
        case 0x02: return "invalid argument";
        case 0x03: return "out of range";
        case 0x04: return "hardware error";
        case 0x05: return "not allowed";
        case 0x06: return "invalid feature index";
        case 0x07: return "invalid function id";
        case 0x08: return "busy";
        case 0x09: return "unsupported";
        default:   return "unrecognized error";
    }
}

static bool is_expected_reply(const uint8_t *report, CFIndex len)
{
    if (len < 5 || (report[0] != 0x10 && report[0] != 0x11))
        return false;

    if (report[2] == BACKLIGHT_FEATURE_INDEX &&
        report[3] == expected_function_swid)
        return true;

    if (len >= 6 && report[2] == 0xFF &&
        report[3] == BACKLIGHT_FEATURE_INDEX &&
        report[4] == expected_function_swid)
        return true;

    return false;
}

static void input_report_callback(void *context,
                                  IOReturn result,
                                  void *sender,
                                  IOHIDReportType type,
                                  uint32_t report_id,
                                  uint8_t *report,
                                  CFIndex report_length)
{
    (void)context;
    (void)sender;
    (void)type;
    (void)report_id;

    if (result != kIOReturnSuccess || !waiting_for_reply)
        return;

    if (!is_expected_reply(report, report_length))
        return;

    print_hex("RX: ", report, report_length);

    if (report_length >= 6 && report[2] == 0xFF) {
        printf("HID++ error: 0x%02X (%s)\n",
               report[5], hidpp20_error_name(report[5]));
    }

    got_reply = true;
    waiting_for_reply = false;
    CFRunLoopStop(CFRunLoopGetCurrent());
}

static bool send_and_wait(IOHIDDeviceRef device,
                          const uint8_t report[7],
                          uint8_t function_swid,
                          double timeout_seconds)
{
    expected_function_swid = function_swid;
    got_reply = false;
    waiting_for_reply = true;

    print_hex("TX: ", report, 7);

    IOReturn ret = IOHIDDeviceSetReport(device,
                                        kIOHIDReportTypeOutput,
                                        report[0],
                                        report,
                                        7);
    if (ret != kIOReturnSuccess) {
        printf("IOHIDDeviceSetReport failed: 0x%08X\n", ret);
        waiting_for_reply = false;
        return false;
    }

    CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + timeout_seconds;
    while (!got_reply && CFAbsoluteTimeGetCurrent() < deadline) {
        double remaining = deadline - CFAbsoluteTimeGetCurrent();
        if (remaining <= 0)
            break;
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, remaining, true);
    }

    waiting_for_reply = false;
    if (!got_reply) {
        printf("RX: timeout (no matching HID++ reply)\n");
        return false;
    }
    return true;
}

static IOHIDDeviceRef copy_k811(void)
{
    IOHIDManagerRef manager =
        IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager)
        return NULL;

    int vid = LOGITECH_VID;
    int pid = K811_PID;
    CFNumberRef vid_number =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid);
    CFNumberRef pid_number =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);

    const void *keys[] = {
        CFSTR(kIOHIDVendorIDKey),
        CFSTR(kIOHIDProductIDKey)
    };
    const void *values[] = { vid_number, pid_number };

    CFDictionaryRef matching = CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    IOHIDManagerSetDeviceMatching(manager, matching);
    IOReturn open_result = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);

    CFRelease(matching);
    CFRelease(vid_number);
    CFRelease(pid_number);

    if (open_result != kIOReturnSuccess) {
        printf("IOHIDManagerOpen failed: 0x%08X\n", open_result);
        CFRelease(manager);
        return NULL;
    }

    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    IOHIDDeviceRef result = NULL;

    if (devices && CFSetGetCount(devices) > 0) {
        CFIndex count = CFSetGetCount(devices);
        const void **items = calloc((size_t)count, sizeof(void *));
        if (items) {
            CFSetGetValues(devices, items);
            for (CFIndex i = 0; i < count; ++i) {
                IOHIDDeviceRef device = (IOHIDDeviceRef)items[i];
                CFTypeRef usage_page_ref =
                    IOHIDDeviceGetProperty(device, CFSTR(kIOHIDPrimaryUsagePageKey));
                CFTypeRef usage_ref =
                    IOHIDDeviceGetProperty(device, CFSTR(kIOHIDPrimaryUsageKey));
                int usage_page = 0;
                int usage = 0;
                if (usage_page_ref && CFGetTypeID(usage_page_ref) == CFNumberGetTypeID())
                    CFNumberGetValue((CFNumberRef)usage_page_ref,
                                     kCFNumberIntType, &usage_page);
                if (usage_ref && CFGetTypeID(usage_ref) == CFNumberGetTypeID())
                    CFNumberGetValue((CFNumberRef)usage_ref,
                                     kCFNumberIntType, &usage);

                if (usage_page == 0x0001 && usage == 0x0006) {
                    result = device;
                    CFRetain(result);
                    break;
                }
            }
            free(items);
        }
    }

    if (devices)
        CFRelease(devices);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
    return result;
}

int main(void)
{
    IOHIDDeviceRef device = copy_k811();
    if (!device) {
        fprintf(stderr, "K811 not found. Is it connected?\n");
        return 1;
    }

    IOReturn ret = IOHIDDeviceOpen(device, kIOHIDOptionsTypeSeizeDevice);
    if (ret != kIOReturnSuccess) {
        fprintf(stderr,
                "IOHIDDeviceOpen failed: 0x%08X\n"
                "Quit/disable Karabiner-Elements and try again.\n",
                ret);
        CFRelease(device);
        return 1;
    }

    memset(input_buffer, 0, sizeof(input_buffer));
    IOHIDDeviceRegisterInputReportCallback(device,
                                           input_buffer,
                                           sizeof(input_buffer),
                                           input_report_callback,
                                           NULL);
    IOHIDDeviceScheduleWithRunLoop(device,
                                   CFRunLoopGetCurrent(),
                                   kCFRunLoopDefaultMode);

    const uint8_t get_backlight[7] = {
        0x10, 0xFF, BACKLIGHT_FEATURE_INDEX, 0x0D, 0x00, 0x00, 0x00
    };

    const uint8_t set_backlight_off[7] = {
        0x10, 0xFF, BACKLIGHT_FEATURE_INDEX, 0x1E, 0x00, 0x00, 0x00
    };

    printf("K811 BACKLIGHT (0x1981, feature index 0x08) probe\n\n");
    printf("GET current value\n");
    (void)send_and_wait(device, get_backlight, 0x0D, 1.0);

    printf("\nSET value 0 (Off)\n");
    (void)send_and_wait(device, set_backlight_off, 0x1E, 1.0);

    printf("\nGET value again\n");
    (void)send_and_wait(device, get_backlight, 0x0D, 1.0);

    IOHIDDeviceUnscheduleFromRunLoop(device,
                                     CFRunLoopGetCurrent(),
                                     kCFRunLoopDefaultMode);
    IOHIDDeviceClose(device, kIOHIDOptionsTypeSeizeDevice);
    CFRelease(device);
    return 0;
}
