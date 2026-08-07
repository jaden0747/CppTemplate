#pragma once

// ImTui's Windows ncurses backend (src/imtui-impl-ncurses.cpp) includes
// <pdcurses.h>, but ConanCenter's pdcurses package only installs the
// conventional <curses.h>. This header is on the include path ahead of the
// Conan package's include dir so that #include resolves.
#include <curses.h>
