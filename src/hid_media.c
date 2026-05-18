#include "hid_media.h"
#include "tusb.h"

static uint16_t _current_usage = 0;

void hid_media_send(uint16_t usage) {
  if (!tud_hid_ready()) return;
  _current_usage = usage;
  tud_hid_report(0, &_current_usage, sizeof(_current_usage));
}

void hid_media_release(void) {
  if (!tud_hid_ready()) return;
  _current_usage = 0;
  tud_hid_report(0, &_current_usage, sizeof(_current_usage));
}

// Called by TinyUSB when host sends GET_REPORT
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen) {
  (void) instance; (void) report_id; (void) report_type; (void) reqlen;
  buffer[0] = (uint8_t)(_current_usage & 0xFF);
  buffer[1] = (uint8_t)(_current_usage >> 8);
  return 2;
}

// Called by TinyUSB when host sends SET_REPORT (not used for consumer control)
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize) {
  (void) instance; (void) report_id; (void) report_type;
  (void) buffer; (void) bufsize;
}
