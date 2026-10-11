#pragma once
#include "image.hpp"
struct PhotoEditorStyle {COLORREF background=RGB(20,24,31),surface=RGB(32,38,47),text=RGB(243,245,250),muted=RGB(160,170,184),accent=RGB(48,180,240);int radius=8;bool animations=true;};
struct PhotoEditorResult {std::shared_ptr<Picture> picture;fs::path saved;};
PhotoEditorResult showPhotoEditor(HWND owner,std::shared_ptr<Picture> picture,const fs::path& original,const PhotoEditorStyle& style);
bool photoEditorSelfTest(HWND owner);
