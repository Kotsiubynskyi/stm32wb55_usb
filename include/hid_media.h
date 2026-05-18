#pragma once
#include <stdint.h>

// Send a Consumer Control key press then release.
// Use HID_USAGE_CONSUMER_* constants from tinyusb/src/class/hid/hid.h,
// e.g. HID_USAGE_CONSUMER_PLAY_PAUSE (0x00CD), HID_USAGE_CONSUMER_STOP (0x00B7),
//      HID_USAGE_CONSUMER_SCAN_NEXT_TRACK (0x00B5), HID_USAGE_CONSUMER_MUTE (0x00E2)
void hid_media_send(uint16_t usage);
void hid_media_release(void);
