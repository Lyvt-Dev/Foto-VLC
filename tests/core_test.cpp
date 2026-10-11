#include "core.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); ++checks; } while(false)
int main() {
    int checks=0;
    CHECK(foto::recognized("C:\\Photos\\Bild.PNG"));
    CHECK(foto::recognized("bild.emf+"));
    CHECK(foto::recognized("scan.3fr"));
    CHECK(foto::recognized("project.psdc"));
    CHECK(foto::recognized("game.bntx"));
    CHECK(!foto::recognized("photo.png.exe"));
    CHECK(!foto::recognized("folder.jpg/no-extension"));
    CHECK(!foto::recognized("readme.txt"));
    CHECK(foto::nativeFormat("jpeg"));
    CHECK(!foto::nativeFormat("afphoto"));
    CHECK(foto::extensions().size()==197);
    CHECK(foto::recognized("HDR-image.EXR"));
    CHECK(foto::quote(L"a b")==L"\"a b\"");
    CHECK(foto::quote(L"a\"b")==L"\"a\\\"b\"");
    CHECK(foto::quote(L"C:\\folder\\")==L"\"C:\\folder\\\\\"");
    CHECK(foto::neighbor(0,-1,3)==2);
    CHECK(foto::neighbor(2,1,3)==0);
    CHECK(foto::neighbor(0,-8,3)==1);
    CHECK(foto::neighbor(0,1,0)==0);
    CHECK(foto::clampZoom(100)==32);
    CHECK(foto::clampZoom(0)==0.01);
    const std::vector<std::vector<int>> expected={{1,2,3,4,5,6},{3,2,1,6,5,4},{6,5,4,3,2,1},{4,5,6,1,2,3},{1,4,2,5,3,6},{4,1,5,2,6,3},{6,3,5,2,4,1},{3,6,2,5,1,4}};
    for(int orientation=1;orientation<=8;++orientation) {
        unsigned w=3,h=2;std::vector<std::uint8_t> pixels(24);
        for(int i=0;i<6;++i)pixels[i*4]=std::uint8_t(i+1);
        CHECK(foto::orientBGRA(pixels,w,h,orientation));
        CHECK(w==(orientation>=5?2u:3u)&&h==(orientation>=5?3u:2u));
        for(int i=0;i<6;++i)CHECK(pixels[i*4]==expected[orientation-1][i]);
    }
    unsigned w=2,h=2;std::vector<std::uint8_t> invalid(1);CHECK(!foto::orientBGRA(invalid,w,h,6));
    std::cout << checks << " checks passed\n";
}
