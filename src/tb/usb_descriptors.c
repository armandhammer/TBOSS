/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original callbacks. cafe:4069 is an unallocated test identity (the SRAM bench
 * images use cafe:4048), so Windows lists this firmware under its own name. */
#include "version.h"
#include "tusb.h"
const tusb_desc_device_t tb_device_descriptor = {
    .bLength = 18, .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
    .bMaxPacketSize0 = 64, .idVendor = 0xcafe, .idProduct = 0x4069, .bcdDevice = TB_USB_BCD,
    .iManufacturer = 1, .iProduct = 2, .bNumConfigurations = 1
};
/* 2.0.6: two ports (cables, midi_in.h): the board's, unnamed as before (Windows keeps calling it
 * TheoryBoard MIDI), and the editor's, its jacks named by string 3. */
#define TB_CONFIG_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_HEAD_LEN + 2 * TUD_MIDI_DESC_JACK_LEN + 2 * TUD_MIDI_DESC_EP_LEN(2))
const uint8_t tb_config_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TB_CONFIG_LEN, 0, 100),
    TUD_MIDI_DESC_HEAD(0, 0, 2),
    TUD_MIDI_DESC_JACK_DESC(1, 0),
    TUD_MIDI_DESC_JACK_DESC(2, 3),
    TUD_MIDI_DESC_EP(0x01, 64, 2), TUD_MIDI_JACKID_IN_EMB(1), TUD_MIDI_JACKID_IN_EMB(2),
    TUD_MIDI_DESC_EP(0x81, 64, 2), TUD_MIDI_JACKID_OUT_EMB(1), TUD_MIDI_JACKID_OUT_EMB(2)
};
_Static_assert(sizeof tb_config_descriptor == TB_CONFIG_LEN, "configuration descriptor length");
uint8_t const *tud_descriptor_device_cb(void) { return (const uint8_t *)&tb_device_descriptor; }
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) { return index == 0 ? tb_config_descriptor : NULL; }
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    static uint16_t data[32]; (void)langid;
    if (index == 0) { data[0] = 0x0304; data[1] = 0x0409; return data; }
    const char *text = index == 1 ? "TheoryBoard project" : index == 2 ? "TheoryBoard MIDI" :
                       index == 3 ? "TheoryBoard Editor" : NULL;
    if (!text) return NULL;
    unsigned n = 0;
    while (n < 31u && text[n]) { data[n + 1u] = (uint8_t)text[n]; ++n; }
    data[0] = (uint16_t)(0x0300u | ((n + 1u) * 2u));
    return data;
}
