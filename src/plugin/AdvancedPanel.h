#pragma once

// The former Advanced panel is now the Studio view (StudioView.h). The editor and its tests
// still name it AdvancedPanel; the alias keeps that code unchanged.
#include "StudioView.h"

namespace cs80::ui
{
using AdvancedPanel = StudioView;
} // namespace cs80::ui
