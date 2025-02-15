/**
 * @file footer_items_nozzle_bed.cpp
 */

#include "footer_items_nozzle_bed.hpp"
#include "marlin_client.hpp"
#include "filament.hpp"
#include "png_resources.hpp"
#include "i18n.h"
#include "config_features.h"
#include "printers.h"
#include "footer_eeprom.hpp"
#include <option/has_toolchanger.h>

#if ENABLED(MODULAR_HEATBED)
    #include <puppies/modular_bed.hpp>
#endif
#if HAS_TOOLCHANGER()
    #include <puppies/Dwarf.hpp>
    #include "Marlin/src/module/prusa/toolchanger.h"
#endif /*HAS_TOOLCHANGER()*/

FooterItemNozzle::FooterItemNozzle(window_t *parent)
    : AddSuperWindow<FooterItemHeater>(parent, &png::nozzle_16x16, static_makeView, static_readValue) {
}

FooterItemBed::FooterItemBed(window_t *parent)
    : AddSuperWindow<FooterItemHeater>(parent, &png::heatbed_16x16, static_makeView, static_readValue) {
#if ENABLED(MODULAR_HEATBED)
    icon.Hide();
#endif
    updateValue();
}

FooterItemAllNozzles::FooterItemAllNozzles(window_t *parent)
    : AddSuperWindow<FooterIconText_IntVal>(parent, &png::nozzle_16x16, static_makeView, static_readValue) {
#if HAS_TOOLCHANGER()
    icon.Hide();
#endif /*HAS_TOOLCHANGER()*/
}

uint FooterItemAllNozzles::nozzle_n = 0;

footer::ItemDrawType FooterItemAllNozzles::GetDrawType() {
    return footer::eeprom::GetItemDrawType();
}

void FooterItemBed::unconditionalDraw() {
    super::unconditionalDraw();

#if ENABLED(MODULAR_HEATBED)
    for (int x = 0; x < X_HBL_COUNT; x++) {
        for (int y = 0; y < Y_HBL_COUNT; y++) {
            uint16_t idx_mask = 1 << buddy::puppies::modular_bed.idx(x, y);
            bool enabled = last_enabled_bedlet_mask & idx_mask;
            bool warm = last_warm_bedlet_mask & idx_mask;

            if (enabled) {
                display::FillRect(
                    Rect16(icon.Left() + x * 4, icon.Top() + icon.Height() - 4 - (y * 4), 3, 3),
                    COLOR_ORANGE);
            } else if (warm) {
                uint px = icon.Left() + x * 4;
                uint py = icon.Top() + icon.Height() - 4 - (y * 4);

                display::SetPixel(point_ui16_t(px + 1, py), COLOR_ORANGE);
                display::SetPixel(point_ui16_t(px + 1, py + 1), COLOR_ORANGE);
                display::SetPixel(point_ui16_t(px + 1, py + 2), COLOR_ORANGE);
                display::SetPixel(point_ui16_t(px, py + 1), COLOR_ORANGE);
                display::SetPixel(point_ui16_t(px + 2, py + 1), COLOR_ORANGE);
            } else {
                display::FillRect(
                    Rect16(icon.Left() + x * 4, icon.Top() + icon.Height() - 4 - (y * 4), 3, 3),
                    COLOR_GRAY);
            }
        }
    }
#endif
}

changed_t FooterItemBed::updateValue() {
    changed_t ret = super::updateValue();
#if ENABLED(MODULAR_HEATBED)
    bool is_heating = marlin_vars()->target_bed > 0;

    // if not heating, act as no heatbedlet is activated
    uint16_t enabled_bedlet_mask = is_heating ? marlin_vars()->enabled_bedlet_mask : 0;
    if (enabled_bedlet_mask != last_enabled_bedlet_mask) {
        last_enabled_bedlet_mask = enabled_bedlet_mask;
        ret = changed_t::yes;
    }

    uint16_t warm_bedlet_mask = 0;
    for (int idx = 0; idx < X_HBL_COUNT * Y_HBL_COUNT; idx++) {
        if (buddy::puppies::modular_bed.get_temp(idx) > COLD)
            warm_bedlet_mask |= 1 << idx;
    }

    if (last_warm_bedlet_mask != warm_bedlet_mask) {
        last_warm_bedlet_mask = warm_bedlet_mask;
        ret = changed_t::yes;
    }

#endif
    // It would seem that returning changed_t::yes is the proper way to
    // trigger an update. However, that doesn't work. Something to investigate.
    if (ret == changed_t::yes)
        Invalidate();
    return ret;
}

void FooterItemAllNozzles::unconditionalDraw() {
    super::unconditionalDraw();

#if HAS_TOOLCHANGER()
    const uint16_t column_size = icon.Width() / NOZZLES_COUNT; // 3 px per nozzle, 2 px column + 1 px space

    // White mark above currently shown tool
    display::FillRect(
        Rect16(icon.Left() + nozzle_n * column_size, icon.Top(), column_size + 1, 2),
        COLOR_WHITE);

    // Individual nozzles
    for (uint nozzle = 0; nozzle < NOZZLES_COUNT; nozzle++) {
        if (buddy::puppies::dwarfs[nozzle].is_enabled() == false) {
            continue;
        }

        // Rectangle as high as temperature (can overwrite the white mark)
        const uint gray_column_max = (static_cast<uint>(COLD) * icon.Height() + (HEATER_XL_HOTEND_MAXTEMP / 2)) / HEATER_XL_HOTEND_MAXTEMP;
        uint column_height = (static_cast<uint>(marlin_vars()->hotend(nozzle).temp_nozzle) * icon.Height() + (HEATER_XL_HOTEND_MAXTEMP / 2)) / HEATER_XL_HOTEND_MAXTEMP;
        column_height = std::clamp<uint>(column_height, 0, icon.Height());
        uint gray_column_height = std::clamp<uint>(column_height, 0, gray_column_max);

        // Gray column down
        display::FillRect(
            Rect16(icon.Left() + nozzle * column_size + 1, icon.Top() + icon.Height() - gray_column_height, column_size - 1, gray_column_height),
            COLOR_GRAY);
        // Orange column up
        if (column_height > gray_column_max) {
            display::FillRect(
                Rect16(icon.Left() + nozzle * column_size + 1, icon.Top() + icon.Height() - column_height, column_size - 1, column_height - gray_column_max),
                COLOR_ORANGE);
        }
    }
#endif /*HAS_TOOLCHANGER()*/
}

changed_t FooterItemAllNozzles::updateValue() {
    changed_t ret = super::updateValue();

    // It would seem that returning changed_t::yes is the proper way to
    // trigger an update. However, that doesn't work. Something to investigate.
    if (ret == changed_t::yes)
        Invalidate();
    return ret;
}

int FooterItemNozzle::static_readValue() {
    static const uint cold = 45;

    const uint current = marlin_vars()->active_hotend().temp_nozzle;
    const uint target = marlin_vars()->active_hotend().target_nozzle;
    const uint display = marlin_vars()->active_hotend().display_nozzle;
#if HAS_TOOLCHANGER()
    const bool disabled = marlin_vars()->active_extruder == PrusaToolChanger::MARLIN_NO_TOOL_PICKED;
#else
    constexpr bool disabled = false;
#endif

    HeatState state = getState(current, target, display, cold);
    StateAndTemps temps(state, current, display, disabled);
    return temps.ToInt();
}

int FooterItemBed::static_readValue() {
    uint current = marlin_vars()->temp_bed;
    uint target = marlin_vars()->target_bed;

    HeatState state = getState(current, target, target, COLD); // display == target will disable green blinking preheat
    StateAndTemps temps(state, current, target, false);
    return temps.ToInt();
}

int FooterItemAllNozzles::static_readValue() {
#if HAS_TOOLCHANGER()
    /// Keep displayed value until switch_gui_time, so there is less flicker
    static uint keep_value = static_cast<uint16_t>(marlin_vars()->hotend(0).temp_nozzle);

    ///< gui::GetTick() of last change of nozzle_n
    static uint32_t switch_gui_time = gui::GetTick();

    // Wait for CYCLE_TIME
    uint32_t now = gui::GetTick();
    if ((now - switch_gui_time) > CYCLE_TIME) {
        switch_gui_time = now;

        // Switch to next enabled nozzle, will also change icon
        do {
            if (++nozzle_n >= NOZZLES_COUNT) {
                nozzle_n = 0;
            }
        } while (buddy::puppies::dwarfs[nozzle_n].is_enabled() == false);

        // Update shown tool and temperature
        keep_value = (nozzle_n << 16) | static_cast<uint16_t>(marlin_vars()->hotend(nozzle_n).temp_nozzle);
    }

    return keep_value; // Return nozzle number in higher 16 bits and shown temperature in lower 16 bits
#else                  /*HAS_TOOLCHANGER()*/
    return static_cast<uint16_t>(marlin_vars()->active_hotend().temp_nozzle); // Nozzle 0 temperature
#endif                 /*HAS_TOOLCHANGER()*/
}

// This methods cannot be one - need separate buffers
string_view_utf8 FooterItemNozzle::static_makeView(int value) {
    static std::array<char, 10> buff;
    return static_makeViewIntoBuff(value, buff);
}

string_view_utf8 FooterItemBed::static_makeView(int value) {
    static std::array<char, 10> buff;
    return static_makeViewIntoBuff(value, buff);
}

string_view_utf8 FooterItemAllNozzles::static_makeView(int value) {
    static constexpr const char *left_aligned_str = "T%u:%u\177C";
    static constexpr const char *const_size_str = "T%u:%3u\177C";

    static std::array<char, 10> buff;

    const uint nozzle_n = value >> 16;
    const uint temperature = std::clamp(int(value & 0xffff), 0, 999);

    const char *const str = (GetDrawType() == footer::ItemDrawType::Static) ? const_size_str : left_aligned_str;
    int printed_chars = snprintf(buff.data(), buff.size(), str, nozzle_n + 1, temperature);

    if (printed_chars <= 0) {
        buff[0] = '\0';
    } else { // Dynamic is not allowed as it changes each second
        //left_aligned print need to end with spaces ensure fixed size
        *(buff.end() - 1) = '\0';
        for (; size_t(printed_chars) < buff.size() - 1; ++printed_chars) {
            buff[printed_chars] = ' ';
        }
    }
    return string_view_utf8::MakeRAM((const uint8_t *)buff.data());
}
