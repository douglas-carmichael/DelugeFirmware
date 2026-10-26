// Whether the firmware has the LXR drum synth engine's DSP: the FEATURE_LXR_ENGINE option in the top-level
// CMakeLists.txt, on unless switched off. Without the DSP, sounds that use the engine still load and save, but play
// silently.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifndef FEATURE_LXR_ENGINE
#define FEATURE_LXR_ENGINE 1
#endif
