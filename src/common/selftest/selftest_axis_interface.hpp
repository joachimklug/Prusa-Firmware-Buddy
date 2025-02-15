/**
 * @file selftest_axis_interface.hpp
 * @author Radek Vana
 * @brief Set of functions adding functionality to selftest classes
 * Could be solved with multiple inheritance (did not want that)
 * or Interface (C++ does not have)
 * @date 2021-09-24
 */
#pragma once
#include <cstdint>
#include "selftest_axis_config.hpp"

class IPartHandler;

namespace selftest {
inline constexpr size_t axis_count = 3;

/**
 * @param separate set true to show progress for each axis separately, gives config_axis.axis to GUI
 */
bool phaseAxis(IPartHandler *&m_pAxis, const AxisConfig_t &config_axis, bool separate = false);

};
