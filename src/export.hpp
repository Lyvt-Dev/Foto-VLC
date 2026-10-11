#pragma once
#include "image.hpp"
enum class ExportFormat {PNG,JPEG,BMP,TIFF,WebP};
const wchar_t* exportExtension(ExportFormat format);
// Saves a raster copy atomically; input identity is checked before any write.
bool exportPicture(const Picture& picture,const fs::path& original,const fs::path& target,
                   ExportFormat format,int rotation,bool mirrored,unsigned quality=95);
bool copyPicture(const Picture& picture,int rotation,bool mirrored);
