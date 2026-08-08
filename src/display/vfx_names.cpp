#include "vfx.h"

const char *vfxModeName(VfxMode mode) {
  switch (mode) {
    case VfxMode::Bars32:
      return "Bars 32";
    case VfxMode::Log12:
      return "Log 12";
    case VfxMode::Mirror:
      return "Mirror";
    case VfxMode::VuMeter:
      return "VU Meter";
    case VfxMode::Waterfall:
      return "Waterfall";
    case VfxMode::Rainbow:
      return "Rainbow";
    case VfxMode::LinePeaks:
      return "Line Peaks";
    case VfxMode::Bounce:
      return "Bounce";
    case VfxMode::Dot:
      return "Dot";
    case VfxMode::Glow:
      return "Glow";
    case VfxMode::Ring:
      return "Ring";
    default:
      return "?";
  }
}
