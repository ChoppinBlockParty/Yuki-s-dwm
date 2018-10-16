#pragma once

// 1 means respect size hints in tiled resizals
#define DWM_RESIZE_HINTS 1

// 0 means no systray
#define DWM_HAS_SYSTRAY 1
// 0: sloppy systray follows selected monitor, >0: pin systray to monitor X
#define DWM_SYSTRAY_PINNING 0
// systray spacing
#define DWM_SYSTRAY_SPACING 2
// 1: if pinning fails, display systray on the first monitor, 0: display systray on the last monitor
#define DWM_SYSTRAY_PINNING_FAIL_FIRST 1
