/*
 * k811-fkeys.c  —  IOKit version for macOS Tahoe (HIDShim-aware)
 *
 * Build:
 *   cc ~/k811-fkeys.c -o ~/k811-fkeys \
 *       -framework IOKit -framework CoreFoundation
 *
 * No hidapi dependency needed.
 *
 * Usage:
 *   sudo ~/k811-fkeys on    <- standard F-keys
 *   sudo ~/k811-fkeys off   <- media keys (hardware default)
 */

#include <stdio.h>
#include <string.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <CoreFoundation/CoreFoundation.h>

#define LOGITECH_VID  0x046D
#define K811_PID      0xB317

static void send_report(IOHIDDeviceRef device, uint8_t mode) {
    uint8_t buf[7] = { 0x10, 0xFF, 0x06, 0x14, mode, 0x00, 0x00 };

    IOReturn ret = IOHIDDeviceOpen(device, kIOHIDOptionsTypeSeizeDevice);
    if (ret != kIOReturnSuccess) {
        printf("IOHIDDeviceOpen failed: 0x%08x\n", ret);
        return;
    }

    ret = IOHIDDeviceSetReport(device,
                               kIOHIDReportTypeOutput,
                               buf[0],
                               buf, sizeof(buf));
    if (ret != kIOReturnSuccess)
        printf("SetReport failed: 0x%08x\n", ret);
    else
        printf("Done.\n");

    IOHIDDeviceClose(device, kIOHIDOptionsTypeSeizeDevice);
}

int main(int argc, char *argv[]) {
    if (argc != 2 || (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0)) {
        fprintf(stderr, "Usage: %s on|off\n", argv[0]);
        return 1;
    }
    uint8_t mode = (strcmp(argv[1], "on") == 0) ? 0x00 : 0x01;

    IOHIDManagerRef mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

    CFNumberRef vid_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType,
                                        &(int){LOGITECH_VID});
    CFNumberRef pid_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType,
                                        &(int){K811_PID});
    CFDictionaryRef match = CFDictionaryCreate(kCFAllocatorDefault,
        (const void *[]){ CFSTR(kIOHIDVendorIDKey), CFSTR(kIOHIDProductIDKey) },
        (const void *[]){ vid_num, pid_num },
        2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    IOHIDManagerSetDeviceMatching(mgr, match);
    IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone);

    CFSetRef devices = IOHIDManagerCopyDevices(mgr);
    if (!devices || CFSetGetCount(devices) == 0) {
        fprintf(stderr, "K811 not found. Is it connected?\n");
        return 1;
    }

    CFSetApplyFunction(devices, (CFSetApplierFunction)send_report,
                       (void *)(uintptr_t)mode);

    CFRelease(devices);
    CFRelease(match);
    CFRelease(vid_num);
    CFRelease(pid_num);
    IOHIDManagerClose(mgr, kIOHIDOptionsTypeNone);
    CFRelease(mgr);
    return 0;
}
