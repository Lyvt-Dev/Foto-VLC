#pragma once
#include <windows.h>
#include <optional>
#include <utility>
namespace paf {
std::optional<std::pair<unsigned,unsigned>> showResizeDialog(HWND owner,unsigned width,unsigned height,COLORREF background,COLORREF foreground);
bool resizeDialogSelfTest(HWND owner);
}
