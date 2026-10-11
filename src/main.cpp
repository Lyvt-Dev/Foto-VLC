#include "image.hpp"
#include "core.hpp"
#include "layout.hpp"
#include "version.h"
#include <windowsx.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <deque>
#include <map>
#include <set>
#include <cmath>
using namespace Gdiplus;
constexpr UINT Done=WM_APP+1,ThumbDone=WM_APP+2;
constexpr wchar_t AppClass[]=L"FotoVLC.Main";
constexpr wchar_t AppTitle[]=L"Foto VLC · " FOTO_VERSION_WSTRING;
enum Action {
    Open=1,OpenFolder,Prev,Next,Fit,Actual,ZoomOut,ZoomIn,Rotate,Full,Info,Formats,Slide,
    Home,Strip,More,About,HelpKeys,HelpFormats,CloseHelp,Reveal,CopyPath,ClearRecent,
    ReleasePage,SourcePage,ErrorDetails,WelcomeOpen,WelcomeFolder,ZoomMenu,
    RecentBase=100,ThumbBase=200
};
enum class Icon {None,Image,Folder,Left,Right,Fit,Actual,Minus,Plus,Rotate,Full,Info,Strip,Play,Pause,More,Home,Close,Help,External,Copy};
struct Control {
    HWND window=nullptr;
    int id=0;
    foto::Box box;
    std::wstring caption,tip;
    Icon icon=Icon::None;
    bool visible=false,enabled=true,active=false,primary=false;
    size_t item=0;
};
struct Preferences {
    bool details=false,strip=true;
    int width=1280,height=860;
    std::vector<fs::path> recent;
    void read(const fs::path& file) {
        if(file.empty())return;
        details=GetPrivateProfileIntW(L"View",L"Details",0,file.c_str())!=0;
        strip=GetPrivateProfileIntW(L"View",L"Filmstrip",1,file.c_str())!=0;
        width=std::clamp(int(GetPrivateProfileIntW(L"View",L"Width",1280,file.c_str())),780,2400);
        height=std::clamp(int(GetPrivateProfileIntW(L"View",L"Height",860,file.c_str())),560,1600);
        recent.clear();
        for(int i=0;i<4;++i) {
            wchar_t value[32768]{};
            GetPrivateProfileStringW(L"Recent",std::to_wstring(i).c_str(),L"",value,32768,file.c_str());
            if(*value)recent.emplace_back(value);
        }
    }
    void write(const fs::path& file)const {
        if(file.empty())return;
        std::wostringstream content;
        content<<L"[View]\r\nDetails="<<(details?1:0)<<L"\r\nFilmstrip="<<(strip?1:0)
               <<L"\r\nWidth="<<width<<L"\r\nHeight="<<height<<L"\r\n[Recent]\r\n";
        for(size_t i=0;i<recent.size()&&i<4;++i)content<<i<<L"="<<recent[i].wstring()<<L"\r\n";
        auto temporary=fs::path(file.wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp");
        bool written=false;
        {
            std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
            const unsigned char bom[]={0xff,0xfe};auto text=content.str();
            output.write(reinterpret_cast<const char*>(bom),2);
            output.write(reinterpret_cast<const char*>(text.data()),std::streamsize(text.size()*sizeof(wchar_t)));
            output.flush();written=bool(output);
        }
        if(!written||!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            std::error_code ec;fs::remove(temporary,ec);
        }
    }
    void remember(const fs::path& path) {
        recent.erase(std::remove(recent.begin(),recent.end(),path),recent.end());
        recent.insert(recent.begin(),path);if(recent.size()>4)recent.resize(4);
    }
};
struct Job {fs::path path;unsigned serial;};
struct Completion {unsigned serial;Decode decoded;};
struct ThumbJob {fs::path path;unsigned folder;};
struct ThumbResult {fs::path path;unsigned folder;std::shared_ptr<Picture> picture;};
void refresh();
void command(int id);
void layoutControls();
LRESULT CALLBACK wndProc(HWND,UINT,WPARAM,LPARAM);

std::shared_ptr<Picture> thumbnail(const fs::path& path) {
    auto decoded=wicLoad(path,160);
    if(decoded.image)return decoded.image;
    std::error_code ec;
    if(fs::file_size(path,ec)>16u*1024*1024 || ec)return {};
    decoded=embeddedLoad(path);if(!decoded.image)return {};
    auto& original=*decoded.image;
    double scale=std::min({1.0,160.0/original.width,160.0/original.height});
    auto result=std::make_shared<Picture>();result->width=std::max(1u,UINT(original.width*scale));result->height=std::max(1u,UINT(original.height*scale));
    result->pixels.resize(size_t(result->width)*result->height*4);
    Bitmap source(original.width,original.height,original.width*4,PixelFormat32bppPARGB,original.pixels.data());
    Bitmap destination(result->width,result->height,result->width*4,PixelFormat32bppPARGB,result->pixels.data());
    Graphics graphics(&destination);graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics.DrawImage(&source,Rect(0,0,result->width,result->height));return result;
}
struct App {
    HWND window=nullptr,tooltip=nullptr;
    Preferences preferences;
    fs::path settingsPath;
    std::vector<fs::path> files;
    size_t index=0;
    std::shared_ptr<Picture> picture;
    std::wstring error,fileSize,fileDate,notice;
    bool busy=false,info=false,strip=true,fullscreen=false,slideshow=false,fit=true,dragging=false,testing=false;
    int rotation=0,helpPage=-1,hover=0;
    double zoom=1,panX=0,panY=0;
    POINT drag{};
    UINT dpi=96;
    RECT restore{};
    DWORD restoreStyle=0;
    std::map<int,Control> controls;
    std::map<fs::path,std::shared_ptr<Picture>> thumbs;
    std::set<fs::path> requestedThumbs;
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<Job> queued;
    std::optional<Completion> completed;
    std::deque<ThumbJob> thumbQueue;
    std::deque<ThumbResult> thumbResults;
    std::thread worker;
    bool stopping=false;
    unsigned serial=0,folderSerial=0;
    void initSettings() {
        if(testing)return;
        PWSTR local=nullptr;
        if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&local))) {
            auto directory=fs::path(local)/L"FotoVLC";CoTaskMemFree(local);
            std::error_code ec;fs::create_directories(directory,ec);
            if(!ec)settingsPath=directory/L"settings.ini";
        }
        preferences.read(settingsPath);info=preferences.details;strip=preferences.strip;
    }
    void saveSettings() {
        if(testing)return;
        preferences.details=info;preferences.strip=strip;preferences.write(settingsPath);
    }
    void begin() {
        worker=std::thread([this]{
            HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            for(;;) {
                std::optional<Job> task;std::optional<ThumbJob> preview;
                {std::unique_lock<std::mutex> lock(mutex);
                 cv.wait(lock,[this]{return stopping||queued.has_value()||!thumbQueue.empty();});
                 if(stopping)break;
                 if(queued){task=*queued;queued.reset();}
                 else{preview=thumbQueue.front();thumbQueue.pop_front();}}
                if(task) {
                    Decode result;
                    try {result=load(task->path);}catch(const std::exception&){result.error=L"Die Datei konnte nicht vollständig gelesen werden.";}
                    {std::lock_guard<std::mutex> lock(mutex);if(stopping)break;completed=Completion{task->serial,std::move(result)};}
                    PostMessageW(window,Done,0,0);
                } else {
                    std::shared_ptr<Picture> result;
                    try {result=thumbnail(preview->path);}catch(const std::exception&){}
                    {std::lock_guard<std::mutex> lock(mutex);if(stopping)break;thumbResults.push_back({preview->path,preview->folder,std::move(result)});}
                    PostMessageW(window,ThumbDone,0,0);
                }
            }
            if(SUCCEEDED(initialized))CoUninitialize();
        });
    }
    void shutdown() {
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;queued.reset();thumbQueue.clear();}
        cv.notify_one();if(worker.joinable())worker.join();
    }
    void resetFolder() {
        ++folderSerial;thumbs.clear();requestedThumbs.clear();
        std::lock_guard<std::mutex> lock(mutex);thumbQueue.clear();thumbResults.clear();
    }
    void request() {
        if(files.empty())return;
        ++serial;busy=true;error.clear();picture.reset();fit=true;zoom=1;rotation=0;panX=panY=0;helpPage=-1;
        SetWindowTextW(window,(files[index].filename().wstring()+L" — "+AppTitle).c_str());
        std::error_code ec;auto bytes=fs::file_size(files[index],ec);
        std::wostringstream size;
        if(bytes<1024)size<<bytes<<L" Byte";
        else if(bytes<1048576)size<<std::fixed<<std::setprecision(0)<<bytes/1024.0<<L" KB";
        else size<<std::fixed<<std::setprecision(1)<<bytes/1048576.0<<L" MB";
        fileSize=ec?L"Nicht verfügbar":size.str();fileDate=L"Nicht verfügbar";
        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if(GetFileAttributesExW(files[index].c_str(),GetFileExInfoStandard,&attributes)) {
            FILETIME local{};SYSTEMTIME date{};
            if(FileTimeToLocalFileTime(&attributes.ftLastWriteTime,&local)&&FileTimeToSystemTime(&local,&date)) {
                wchar_t formatted[80]{};GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,DATE_SHORTDATE,&date,nullptr,formatted,80,nullptr);fileDate=formatted;
            }
        }
        {std::lock_guard<std::mutex> lock(mutex);queued=Job{files[index],serial};}
        cv.notify_one();SetTimer(window,2,60,nullptr);refresh();
    }
    bool known(const fs::path& p)const {
        auto ext=p.extension().wstring();std::string ascii;
        for(auto c:ext)ascii+=c<128?char(c):'?';
        return foto::recognized("file"+ascii);
    }
    void openPath(const fs::path& incoming) {
        std::error_code ec;auto path=fs::absolute(incoming,ec).lexically_normal();
        if(ec)return;
        bool directory=fs::is_directory(path,ec);
        if(!directory&&!fs::is_regular_file(path,ec)) {
            MessageBoxW(window,L"Die Datei ist nicht mehr an diesem Ort. Öffne sie bitte erneut über »Öffnen«.",L"Datei nicht gefunden",MB_OK|MB_ICONINFORMATION);return;
        }
        auto root=directory?path:path.parent_path();std::vector<fs::path> found;
        fs::directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
        for(;!ec&&it!=end;it.increment(ec)) {
            std::error_code e;if(it->is_regular_file(e)&&known(it->path()))found.push_back(it->path());
        }
        if(!directory&&std::find(found.begin(),found.end(),path)==found.end())found.push_back(path);
        std::sort(found.begin(),found.end(),[](const auto& a,const auto& b){return StrCmpLogicalW(a.filename().c_str(),b.filename().c_str())<0;});
        if(found.empty()) {
            MessageBoxW(window,L"In diesem Ordner wurden keine Bilddateien gefunden. Wähle einen anderen Ordner aus.",L"Keine Bilder im Ordner",MB_OK|MB_ICONINFORMATION);return;
        }
        resetFolder();files=std::move(found);
        auto selected=std::find(files.begin(),files.end(),path);index=selected==files.end()?0:size_t(selected-files.begin());request();
    }
};
App app;

namespace ui {
const Color background(255,17,19,22),surface(255,25,28,32),raised(255,34,38,43),line(255,45,50,56);
const Color text(255,236,239,242),muted(255,159,169,180),faint(255,106,117,129),accent(255,135,218,195);
void fill(Graphics& g,Color color,foto::Box r){SolidBrush brush(color);g.FillRectangle(&brush,r.x,r.y,r.w,r.h);}
void round(Graphics& g,Color color,foto::Box r,float radius=7) {
    if(r.w<=0||r.h<=0)return;
    float d=std::min(radius*2,std::min(r.w,r.h));GraphicsPath path;
    path.AddArc(r.x,r.y,d,d,180,90);path.AddArc(r.x+r.w-d,r.y,d,d,270,90);
    path.AddArc(r.x+r.w-d,r.y+r.h-d,d,d,0,90);path.AddArc(r.x,r.y+r.h-d,d,d,90,90);path.CloseFigure();
    SolidBrush brush(color);g.FillPath(&brush,&path);
}
void label(Graphics& g,const std::wstring& textValue,foto::Box r,float size=13,Color color=text,bool bold=false,bool center=false,bool wrap=false) {
    FontFamily family(L"Segoe UI");Font font(family.GetLastStatus()==Ok?&family:FontFamily::GenericSansSerif(),size,bold?FontStyleBold:FontStyleRegular,UnitPixel);
    SolidBrush brush(color);StringFormat format;format.SetTrimming(StringTrimmingEllipsisCharacter);
    format.SetLineAlignment(StringAlignmentCenter);if(!wrap)format.SetFormatFlags(StringFormatFlagsNoWrap);
    if(center)format.SetAlignment(StringAlignmentCenter);
    g.DrawString(textValue.c_str(),-1,&font,RectF(r.x,r.y,r.w,r.h),&format,&brush);
}
void icon(Graphics& g,Icon type,float x,float y,Color color=text,float size=20) {
    auto state=g.Save();g.TranslateTransform(x,y);g.ScaleTransform(size/24,size/24);
    Pen p(color,1.65f);p.SetStartCap(LineCapRound);p.SetEndCap(LineCapRound);p.SetLineJoin(LineJoinRound);
    auto l=[&](float a,float b,float c,float d){g.DrawLine(&p,a,b,c,d);};
    switch(type) {
    case Icon::Image:g.DrawRectangle(&p,3.0f,4.0f,18.0f,16.0f);g.DrawEllipse(&p,14.0f,7.0f,3.0f,3.0f);l(4,18,9,12);l(9,12,13,16);l(13,16,16,13);l(16,13,20,18);break;
    case Icon::Folder:l(3,7,3,20);l(3,20,21,20);l(21,20,21,7);l(21,7,12,7);l(12,7,10,4);l(10,4,3,4);l(3,4,3,7);break;
    case Icon::Left:l(14,6,8,12);l(8,12,14,18);break;
    case Icon::Right:l(10,6,16,12);l(16,12,10,18);break;
    case Icon::Minus:l(6,12,18,12);break;
    case Icon::Plus:l(6,12,18,12);l(12,6,12,18);break;
    case Icon::Fit:l(3,9,3,4);l(3,4,8,4);l(16,4,21,4);l(21,4,21,9);l(21,15,21,20);l(21,20,16,20);l(8,20,3,20);l(3,20,3,15);g.DrawRectangle(&p,7.0f,8.0f,10.0f,8.0f);break;
    case Icon::Full:l(4,9,4,4);l(4,4,9,4);l(15,4,20,4);l(20,4,20,9);l(20,15,20,20);l(20,20,15,20);l(9,20,4,20);l(4,20,4,15);break;
    case Icon::Rotate:g.DrawArc(&p,5.0f,5.0f,14.0f,14.0f,210.0f,290.0f);l(4,4,4,10);l(4,10,10,10);break;
    case Icon::Info:g.DrawEllipse(&p,3.0f,3.0f,18.0f,18.0f);l(12,11,12,17);l(12,7,12,7.3f);break;
    case Icon::Strip:g.DrawRectangle(&p,2.0f,5.0f,20.0f,14.0f);l(8,5,8,19);l(16,5,16,19);break;
    case Icon::Play:l(8,5,19,12);l(19,12,8,19);l(8,19,8,5);break;
    case Icon::Pause:l(8,5,8,19);l(16,5,16,19);break;
    case Icon::More:for(int i=5;i<=19;i+=7){SolidBrush b(color);g.FillEllipse(&b,float(i-1),11.0f,2.5f,2.5f);}break;
    case Icon::Home:l(3,11,12,3);l(12,3,21,11);l(6,10,6,21);l(6,21,18,21);l(18,21,18,10);break;
    case Icon::Close:l(6,6,18,18);l(6,18,18,6);break;
    case Icon::Help:g.DrawEllipse(&p,3.0f,3.0f,18.0f,18.0f);g.DrawArc(&p,9.0f,7.0f,6.0f,6.0f,180.0f,230.0f);l(12,13,12,14);l(12,17,12,17.3f);break;
    case Icon::External:l(14,4,20,4);l(20,4,20,10);l(20,4,11,13);l(8,5,4,5);l(4,5,4,20);l(4,20,19,20);l(19,20,19,16);break;
    case Icon::Copy:g.DrawRectangle(&p,8.0f,8.0f,12.0f,13.0f);l(15,4,4,4);l(4,4,4,16);break;
    default:break;
    }
    g.Restore(state);
}
void brand(Graphics& g,float x,float y,float size=28) {
    round(g,accent,{x,y,size,size},7);icon(g,Icon::Image,x+5,y+5,background,size-10);
}
void drawPicture(Graphics& g,const Picture& p,foto::Box box,bool crop=false) {
    float scale=crop?std::max(box.w/p.width,box.h/p.height):std::min(box.w/p.width,box.h/p.height);
    float w=p.width*scale,h=p.height*scale;auto saved=g.Save();g.SetClip(RectF(box.x,box.y,box.w,box.h));
    Bitmap bitmap(p.width,p.height,p.width*4,PixelFormat32bppPARGB,const_cast<BYTE*>(p.pixels.data()));
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.DrawImage(&bitmap,RectF(box.x+(box.w-w)/2,box.y+(box.h-h)/2,w,h));g.Restore(saved);
}
}
foto::ViewerLayout viewLayout() {
    RECT r{};GetClientRect(app.window,&r);float scale=app.dpi/96.0f;
    return {r.right/scale,r.bottom/scale,!app.files.empty(),app.info,app.strip};
}
void updateZoom() {
    if(!app.picture)return;
    auto l=viewLayout();auto& p=*app.picture;
    double width=app.rotation%2?p.height:p.width,height=app.rotation%2?p.width:p.height;
    if(app.fit)app.zoom=std::min({1.0,double(l.image.w)/width,double(l.image.h)/height});
    double maxX=std::max(0.0,(width*app.zoom-l.image.w)/2+16);
    double maxY=std::max(0.0,(height*app.zoom-l.image.h)/2+16);
    app.panX=std::clamp(app.panX,-maxX,maxX);app.panY=std::clamp(app.panY,-maxY,maxY);
}
void zoomAt(double factor,float x,float y) {
    if(!app.picture)return;
    updateZoom();auto l=viewLayout();double old=app.zoom;app.fit=false;app.zoom=foto::clampZoom(old*factor);
    double cx=l.image.x+l.image.w/2,cy=l.image.y+l.image.h/2;
    app.panX=x-cx-(x-cx-app.panX)*app.zoom/old;app.panY=y-cy-(y-cy-app.panY)*app.zoom/old;
    refresh();
}
bool shortcut(UINT key) {
    if(key=='O'&&GetKeyState(VK_CONTROL)<0)command(GetKeyState(VK_SHIFT)<0?OpenFolder:Open);
    else if(key==VK_F1)command(HelpKeys);
    else if(key==VK_ESCAPE) {
        if(app.helpPage>=0)command(CloseHelp);else if(app.fullscreen)command(Full);else if(app.slideshow)command(Slide);
    }else if(app.helpPage<0) {
        switch(key) {
        case VK_LEFT:command(Prev);break;case VK_RIGHT:command(Next);break;
        case 'F':command(Fit);break;case '1':command(Actual);break;case 'R':command(Rotate);break;
        case 'I':command(Info);break;case 'T':command(Strip);break;case VK_F11:command(Full);break;
        case VK_SPACE:command(Slide);break;case VK_ADD:case VK_OEM_PLUS:command(ZoomIn);break;
        case VK_SUBTRACT:case VK_OEM_MINUS:command(ZoomOut);break;
        default:return false;
        }
    }else return false;
    return true;
}
LRESULT CALLBACK buttonProc(HWND window,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR id) {
    switch(msg) {
    case WM_MOUSEMOVE: {
        if(app.hover!=int(id)) {int old=app.hover;app.hover=int(id);if(app.controls.count(old))InvalidateRect(app.controls[old].window,nullptr,FALSE);InvalidateRect(window,nullptr,FALSE);}
        TRACKMOUSEEVENT tracking{sizeof(tracking),TME_LEAVE,window,0};TrackMouseEvent(&tracking);break;
    }
    case WM_MOUSELEAVE:if(app.hover==int(id)){app.hover=0;InvalidateRect(window,nullptr,FALSE);}break;
    case WM_SETFOCUS:case WM_KILLFOCUS:InvalidateRect(window,nullptr,FALSE);break;
    case WM_KEYDOWN:
        if(wp==VK_RETURN){command(int(id));return 0;}
        if(wp!=VK_SPACE&&shortcut(UINT(wp)))return 0;
        break;
    case WM_NCDESTROY:RemoveWindowSubclass(window,buttonProc,id);break;
    }
    return DefSubclassProc(window,msg,wp,lp);
}
void place(int id,foto::Box box,const std::wstring& caption,Icon icon=Icon::None,bool enabled=true,bool active=false,bool primary=false,const std::wstring& tip=L"") {
    auto& c=app.controls[id];c.id=id;c.box=box;c.visible=true;c.enabled=enabled;c.active=active;c.primary=primary;c.icon=icon;
    if(!c.window) {
        c.window=CreateWindowExW(0,L"BUTTON",caption.c_str(),WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,0,0,1,1,app.window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        SetWindowSubclass(c.window,buttonProc,UINT_PTR(id),DWORD_PTR(id));
        TOOLINFOW tool{};tool.cbSize=sizeof(tool);tool.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tool.hwnd=app.window;tool.uId=reinterpret_cast<UINT_PTR>(c.window);
        c.tip=tip.empty()?caption:tip;tool.lpszText=c.tip.data();SendMessageW(app.tooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tool));
    }
    if(c.caption!=caption){c.caption=caption;SetWindowTextW(c.window,caption.c_str());}
    auto wantedTip=tip.empty()?caption:tip;
    if(c.tip!=wantedTip) {
        c.tip=wantedTip;TOOLINFOW tool{};tool.cbSize=sizeof(tool);tool.hwnd=app.window;tool.uId=reinterpret_cast<UINT_PTR>(c.window);tool.lpszText=c.tip.data();
        SendMessageW(app.tooltip,TTM_UPDATETIPTEXTW,0,reinterpret_cast<LPARAM>(&tool));
    }
    float scale=app.dpi/96.0f;
    SetWindowPos(c.window,nullptr,int(std::round(box.x*scale)),int(std::round(box.y*scale)),int(std::round(box.w*scale)),int(std::round(box.h*scale)),SWP_NOZORDER|SWP_NOACTIVATE);
    EnableWindow(c.window,enabled);ShowWindow(c.window,SW_SHOWNA);InvalidateRect(c.window,nullptr,FALSE);
}
void scheduleThumbnails(size_t first,size_t count) {
    bool queued=false;
    {std::lock_guard<std::mutex> lock(app.mutex);
     // Only queue visible files. A main-image request always takes priority.
     for(size_t i=first;i<first+count;++i)if(!app.requestedThumbs.count(app.files[i])) {
         app.requestedThumbs.insert(app.files[i]);app.thumbQueue.push_back({app.files[i],app.folderSerial});queued=true;
     }}
    if(queued)app.cv.notify_one();
    if(app.thumbs.size()>48) {
        std::set<fs::path> keep(app.files.begin()+first,app.files.begin()+first+count);
        for(auto it=app.thumbs.begin();it!=app.thumbs.end()&&app.thumbs.size()>32;) {
            if(!keep.count(it->first)){app.requestedThumbs.erase(it->first);it=app.thumbs.erase(it);}else ++it;
        }
    }
}
void layoutControls() {
    if(!app.window)return;
    for(auto& entry:app.controls)entry.second.visible=false;
    auto l=viewLayout();float w=l.width,h=l.height;bool file=!app.files.empty(),viewer=file&&app.helpPage<0,loaded=bool(app.picture)&&!app.busy;
    place(About,{w-98,16,34,32},L"Über Foto VLC",Icon::Help,true,false,false,L"Info & Hilfe (F1)");
    place(More,{w-56,16,36,32},L"Weitere Optionen",Icon::More);
    if(file&&app.helpPage<0) {
        place(Open,{w-252,16,100,32},L"Öffnen",Icon::Image,true,false,true,L"Bild öffnen (Strg+O)");
        place(OpenFolder,{w-144,16,36,32},L"Ordner öffnen",Icon::Folder,true,false,false,L"Ordner öffnen (Strg+Umschalt+O)");
    }
    if(app.helpPage>=0) {
        place(CloseHelp,{24,78,112,34},L"Zurück",Icon::Left);
        float x=std::max(32.0f,(w-800)/2),y=198;
        place(About,{x,y,128,38},L"Über die App",Icon::None,true,app.helpPage==0);
        place(HelpKeys,{x+138,y,132,38},L"Tastenkürzel",Icon::None,true,app.helpPage==1);
        place(HelpFormats,{x+280,y,132,38},L"Dateiformate",Icon::None,true,app.helpPage==2);
        if(app.helpPage==0) {
            place(ReleasePage,{x,426,174,38},L"Versionen & Updates",Icon::External);
            place(SourcePage,{x+184,426,150,38},L"Projekt auf GitHub",Icon::External);
        }else if(app.helpPage==2)place(ReleasePage,{x,462,196,38},L"Vollständige Formatliste",Icon::External);
    }else if(!file) {
        float x=std::max(40.0f,(w-850)/2),y=std::max(132.0f,(h-370)/2);
        place(WelcomeOpen,{x,y+190,166,44},L"Bild öffnen",Icon::Image,true,false,true,L"Ein Bild auswählen (Strg+O)");
        place(WelcomeFolder,{x+178,y+190,150,44},L"Ordner öffnen",Icon::Folder,true,false,false,L"Einen Bilderordner auswählen (Strg+Umschalt+O)");
        float rx=x+450,rw=std::max(250.0f,w-rx-40);
        if(w<960){rx=x;rw=w-2*x;}
        float ry=w<960?y+264:y+36;
        for(size_t i=0;i<app.preferences.recent.size();++i) {
            if(w<960&&i>0)break;
            place(RecentBase+int(i),{rx,ry+34+float(i)*66,rw,58},app.preferences.recent[i].filename().wstring(),Icon::Image,true,false,false,app.preferences.recent[i].wstring());
            app.controls[RecentBase+int(i)].item=i;
        }
    }else {
        place(Home,{18,75,32,28},L"Startseite",Icon::Home,true,false,false,L"Zur Startseite");
        place(Strip,{w-250,74,126,30},L"Filmstreifen",Icon::Strip,true,app.strip,false,L"Filmstreifen anzeigen oder ausblenden (T)");
        place(Info,{w-114,74,94,30},L"Details",Icon::Info,true,app.info,false,L"Bildinformationen anzeigen oder ausblenden (I)");
        float toolbarWidth=452,tx=std::max(8.0f,(l.canvas.w-toolbarWidth)/2),ty=l.canvas.y+l.canvas.h-58;
        place(Fit,{tx+6,ty+5,112,34},L"Anpassen",Icon::Fit,loaded,app.fit,false,L"Bild an das Fenster anpassen (F)");
        place(Actual,{tx+122,ty+5,38,34},L"1:1",Icon::None,loaded,!app.fit&&app.zoom==1,false,L"Originalgröße (1)");
        place(ZoomOut,{tx+172,ty+5,32,34},L"Verkleinern",Icon::Minus,loaded,false,false,L"Verkleinern (−)");
        place(ZoomMenu,{tx+204,ty+5,56,34},std::to_wstring(int(std::round(app.zoom*100)))+L" %",Icon::None,loaded,false,false,L"Zoomstufe auswählen");
        place(ZoomIn,{tx+260,ty+5,32,34},L"Vergrößern",Icon::Plus,loaded,false,false,L"Vergrößern (+)");
        place(Rotate,{tx+304,ty+5,34,34},L"Drehen",Icon::Rotate,loaded,false,false,L"90° nach rechts drehen (R)");
        place(Full,{tx+340,ty+5,34,34},L"Vollbild",Icon::Full,true,app.fullscreen,false,L"Vollbild ein-/ausschalten (F11)");
        place(Slide,{tx+394,ty+5,46,34},app.slideshow?L"Diashow anhalten":L"Diashow starten",app.slideshow?Icon::Pause:Icon::Play,app.files.size()>1,app.slideshow,false,L"Diashow starten oder anhalten (Leertaste)");
        if(app.files.size()>1) {
            place(Prev,{8,l.image.y+l.image.h/2-22,32,44},L"Vorheriges Bild",Icon::Left,true,false,false,L"Vorheriges Bild (←)");
            place(Next,{l.canvas.w-40,l.image.y+l.image.h/2-22,32,44},L"Nächstes Bild",Icon::Right,true,false,false,L"Nächstes Bild (→)");
        }
        if(app.info) {
            place(Reveal,{l.inspector.x+20,l.inspector.y+l.inspector.h-58,156,34},L"Im Explorer zeigen",Icon::External);
            place(CopyPath,{l.inspector.x+184,l.inspector.y+l.inspector.h-58,34,34},L"Dateipfad kopieren",Icon::Copy);
        }
        if(!app.error.empty()) {
            float cx=l.image.x+l.image.w/2,cy=l.image.y+l.image.h/2;
            place(ErrorDetails,{cx-125,cy+72,122,34},L"Fehlerdetails",Icon::None);
            place(Formats,{cx+7,cy+72,122,34},L"Formathilfe",Icon::Help);
        }
        if(app.strip) {
            auto range=foto::filmRange(app.index,app.files.size(),l.filmstrip.w);
            float sx=(l.filmstrip.w-float(range.second)*88+8)/2;
            for(size_t n=0;n<range.second;++n) {
                size_t item=range.first+n;
                place(ThumbBase+int(n),{sx+float(n)*88,l.filmstrip.y+28,80,72},app.files[item].filename().wstring(),Icon::Image,true,item==app.index,false,app.files[item].filename().wstring());
                app.controls[ThumbBase+int(n)].item=item;
            }
            scheduleThumbnails(range.first,range.second);
        }
    }
    for(auto& entry:app.controls)if(!entry.second.visible&&entry.second.window)ShowWindow(entry.second.window,SW_HIDE);
    (void)viewer;
}
void refresh() {
    if(!app.window)return;
    updateZoom();layoutControls();InvalidateRect(app.window,nullptr,FALSE);
}
void drawButton(const DRAWITEMSTRUCT& item) {
    auto it=app.controls.find(int(item.CtlID));if(it==app.controls.end())return;
    auto& c=it->second;float scale=app.dpi/96.0f,w=c.box.w,h=c.box.h;
    Graphics g(item.hDC);g.ScaleTransform(scale,scale);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    Color parent=ui::background;
    if(c.id<RecentBase && c.box.y<64)parent=ui::surface;
    if(c.id>=ThumbBase)parent=ui::surface;
    if(c.id==Reveal||c.id==CopyPath)parent=ui::surface;
    if(c.id>=Fit&&c.id<=Slide&&c.id!=Info&&c.id!=Formats)parent=ui::surface;
    if(c.id==ZoomMenu)parent=ui::surface;
    ui::fill(g,parent,{0,0,w,h});
    bool hot=app.hover==c.id,pressed=(item.itemState&ODS_SELECTED)!=0,focused=(item.itemState&ODS_FOCUS)!=0;
    Color fg=c.enabled?ui::text:ui::faint;
    Color fill=pressed?Color(255,55,62,69):hot?Color(255,43,49,55):parent;
    if(c.active){fill=Color(255,36,60,54);fg=ui::accent;}
    if(c.primary){fill=pressed?Color(255,113,191,170):hot?Color(255,159,230,210):ui::accent;fg=ui::background;}
    if(c.id>=ThumbBase) {
        ui::round(g,c.active?ui::accent:hot?Color(255,72,81,90):ui::line,{0,0,w,54},5);
        ui::round(g,ui::background,{2,2,w-4,50},3);
        if(c.item<app.files.size()) {
            auto thumbnail=app.thumbs.find(app.files[c.item]);
            if(thumbnail!=app.thumbs.end()&&thumbnail->second)ui::drawPicture(g,*thumbnail->second,{3,3,w-6,48});
            else ui::icon(g,Icon::Image,w/2-9,17,ui::faint,18);
        }
        ui::label(g,c.caption,{1,56,w-2,16},10,c.active?ui::accent:ui::muted,false,true);
    }else if(c.id>=RecentBase) {
        ui::round(g,fill,{0,0,w,h},7);ui::round(g,ui::raised,{8,9,40,40},7);ui::icon(g,Icon::Image,18,19,ui::muted);
        ui::label(g,c.caption,{60,7,w-86,23},13,fg,true);
        if(c.item<app.preferences.recent.size())ui::label(g,app.preferences.recent[c.item].parent_path().wstring(),{60,31,w-86,17},11,ui::muted);
        ui::icon(g,Icon::Right,w-24,20,ui::muted,16);
    }else {
        ui::round(g,fill,{0,0,w,h},6);
        bool iconOnly=c.icon!=Icon::None&&w<=48;
        if(iconOnly)ui::icon(g,c.icon,(w-19)/2,(h-19)/2,fg,19);
        else if(c.icon!=Icon::None) {
            ui::icon(g,c.icon,12,(h-18)/2,fg,18);
            ui::label(g,c.caption,{38,0,w-44,h},12,fg,c.primary);
        }else ui::label(g,c.caption,{4,0,w-8,h},12,fg,false,true);
    }
    if(focused) {Pen focus(ui::accent,1);g.DrawRectangle(&focus,2.0f,2.0f,w-4,h-4);}
}
void drawWelcome(Graphics& g,const foto::ViewerLayout& l) {
    float x=std::max(40.0f,(l.width-850)/2),y=std::max(132.0f,(l.height-370)/2);
    ui::round(g,ui::surface,{x,y,52,52},13);ui::icon(g,Icon::Image,x+13,y+13,ui::accent,26);
    ui::label(g,L"Willkommen bei Foto VLC",{x,y+67,380,72},30,ui::text,true,false,true);
    ui::label(g,L"Öffne ein Bild oder einen Ordner.\nDu kannst Dateien auch hierher ziehen.",{x,y+140,360,40},13,ui::muted,false,false,true);
    ui::label(g,L"Strg + O zum Öffnen · F1 für Hilfe",{x,y+242,350,24},11,ui::faint);
    float rx=x+450,rw=std::max(250.0f,l.width-rx-40),ry=y+36;
    if(l.width<960){rx=x;rw=l.width-2*x;ry=y+264;}
    ui::label(g,L"Zuletzt geöffnet",{rx,ry,rw,24},13,ui::text,true);
    if(app.preferences.recent.empty()) {
        ui::round(g,ui::surface,{rx,ry+34,rw,112},9);
        ui::icon(g,Icon::Image,rx+20,ry+56,ui::faint,23);
        ui::label(g,L"Deine letzten Bilder erscheinen hier.",{rx+56,ry+50,rw-76,36},12,ui::muted,false,false,true);
        ui::label(g,L"Nur auf diesem Gerät gespeichert.",{rx+20,ry+103,rw-40,22},11,ui::faint);
    }
}
void drawHelp(Graphics& g,const foto::ViewerLayout& l) {
    float x=std::max(32.0f,(l.width-800)/2),w=std::min(800.0f,l.width-64);
    ui::label(g,L"Info & Hilfe",{x,130,w,48},28,ui::text,true);
    ui::fill(g,ui::line,{x,248,w,1});
    if(app.helpPage==0) {
        ui::brand(g,x,278,48);
        ui::label(g,L"Foto VLC",{x+66,276,w-66,32},25,ui::text,true);
        ui::label(g,L"Version " FOTO_VERSION_WSTRING L"  ·  Windows 64-Bit",{x+66,312,w-66,25},13,ui::accent);
        ui::label(g,L"Ein schneller Bildbetrachter für deine Fotos, Illustrationen und Bilddateien.",{x,355,w,28},14);
        ui::label(g,L"Entwickelt von Lyvt-Dev · MIT-Lizenz",{x,389,w,24},12,ui::muted);
        ui::label(g,L"Originaldateien werden beim Betrachten nicht verändert.",{x,488,w,24},12,ui::muted);
    }else if(app.helpPage==1) {
        const std::pair<const wchar_t*,const wchar_t*> keys[]={
            {L"Strg + O",L"Bild öffnen"},{L"Strg + Umschalt + O",L"Ordner öffnen"},{L"←  /  →",L"Zwischen Bildern blättern"},
            {L"Mausrad / + / −",L"Zoom ändern"},{L"F  /  1",L"Anpassen / Originalgröße"},{L"R",L"Bild nach rechts drehen"},
            {L"F11",L"Vollbild ein-/ausschalten"},{L"Leertaste",L"Diashow starten / anhalten"},{L"I  /  T",L"Details / Filmstreifen"},
            {L"Doppelklick",L"Zwischen Anpassen und 1:1 wechseln"},{L"Ziehen",L"Vergrößertes Bild verschieben"},{L"Esc",L"Hilfe, Vollbild oder Diashow verlassen"},
            {L"Tab / Umschalt + Tab",L"Bedienelemente auswählen"},{L"Enter",L"Ausgewählte Schaltfläche auslösen"}};
        float cw=(w-28)/2;
        for(size_t i=0;i<14;++i) {
            float cx=x+(i/7)*(cw+28),cy=269+float(i%7)*34;
            ui::label(g,keys[i].first,{cx,cy,cw*0.48f,30},11,ui::accent);
            ui::label(g,keys[i].second,{cx+cw*0.48f,cy,cw*0.52f,30},11,ui::text,false,false,true);
        }
    }else {
        ui::label(g,L"Viele Bilder direkt öffnen",{x,274,w,28},18,ui::text,true);
        ui::label(g,L"PNG, JPEG, BMP, GIF, TIFF und ICO sowie SVG-Formen, PSD-Komposite, TGA, PGM/PPM, PIC und QOI.",{x,310,w,42},13,ui::muted,false,false,true);
        ui::label(g,L"Weitere Formate mit zusätzlichen Decodern",{x,362,w,26},16,ui::text,true);
        ui::label(g,L"Installierte Windows-Codecs und optional ImageMagick erweitern die Unterstützung. Proprietäre Projekte sind nicht durchgehend lesbar. Animationen und mehrseitige Dateien zeigen das erste Bild.",{x,393,w,56},12,ui::muted,false,false,true);
        ui::label(g,magickPath().empty()?L"ImageMagick: nicht eingerichtet":L"ImageMagick: verfügbar",{x+214,462,w-214,38},11,ui::muted);
    }
}
void drawInspector(Graphics& g,const foto::ViewerLayout& l) {
    auto r=l.inspector;if(r.w==0)return;
    ui::fill(g,ui::surface,r);ui::fill(g,ui::line,{r.x,r.y,1,r.h});
    float x=r.x+22,y=r.y+22,w=r.w-44;
    ui::label(g,L"Bilddetails",{x,y,w,24},14,ui::text,true);
    ui::label(g,app.files[app.index].filename().wstring(),{x,y+38,w,52},17,ui::text,true,false,true);
    ui::fill(g,ui::line,{x,y+110,w,1});
    auto row=[&](float yy,const wchar_t* name,const std::wstring& value) {
        ui::label(g,name,{x,yy,w,18},11,ui::muted);ui::label(g,value,{x,yy+21,w,23},13);
    };
    row(y+132,L"Abmessungen",app.picture?std::to_wstring(app.picture->originalW)+L" × "+std::to_wstring(app.picture->originalH)+L" px":L"—");
    row(y+196,L"Dateigröße",app.fileSize);
    auto extension=app.files[app.index].extension().wstring();if(!extension.empty())extension.erase(0,1);
    std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return wchar_t(towupper(c));});
    row(y+260,L"Dateiformat",extension.empty()?L"Unbekannt":extension);
    if(r.h>490)row(y+324,L"Zuletzt geändert",app.fileDate);
    if(app.picture&&app.picture->frames>1&&r.h>560)ui::label(g,L"Vorschau: erstes Bild / erste Seite",{x,y+394,w,40},11,ui::muted,false,false,true);
}
void drawViewer(Graphics& g,const foto::ViewerLayout& l) {
    ui::icon(g,Icon::Folder,62,82,ui::faint,15);
    auto folder=app.files[app.index].parent_path().filename().wstring();
    ui::label(g,folder,{86,73,l.width-345,32},12,ui::muted);
    if(app.picture) {
        auto& p=*app.picture;auto r=l.image;
        double rw=app.rotation%2?p.height:p.width,rh=app.rotation%2?p.width:p.height;
        double dw=rw*app.zoom,dh=rh*app.zoom;
        float cx=r.x+r.w/2+float(app.panX),cy=r.y+r.h/2+float(app.panY);
        auto state=g.Save();g.SetClip(RectF(r.x,r.y,r.w,r.h));
        float left=cx-float(dw)/2,top=cy-float(dh)/2;
        ui::fill(g,Color(90,0,0,0),{left+4,top+6,float(dw),float(dh)});
        g.SetClip(RectF(left,top,float(dw),float(dh)),CombineModeIntersect);
        ui::fill(g,Color(255,38,41,45),{left,top,float(dw),float(dh)});
        float sx=std::max(r.x,left),sy=std::max(r.y,top),ex=std::min(r.x+r.w,left+float(dw)),ey=std::min(r.y+r.h,top+float(dh));
        for(int y=int(sy)/12*12;y<ey;y+=12)for(int x=int(sx)/12*12;x<ex;x+=12)
            if(((x/12)+(y/12))%2)ui::fill(g,Color(255,46,50,55),{float(x),float(y),12,12});
        g.TranslateTransform(cx,cy);g.RotateTransform(float(app.rotation*90));
        g.SetInterpolationMode(app.zoom>3?InterpolationModeNearestNeighbor:InterpolationModeHighQualityBicubic);
        Bitmap bitmap(p.width,p.height,p.width*4,PixelFormat32bppPARGB,p.pixels.data());
        g.DrawImage(&bitmap,RectF(float(-double(p.width)*app.zoom/2),float(-double(p.height)*app.zoom/2),float(p.width*app.zoom),float(p.height*app.zoom)));
        g.Restore(state);
    }else {
        auto r=l.image;float cx=r.x+r.w/2,cy=r.y+r.h/2;
        if(app.busy) {
            Pen spin(ui::accent,2.5f);spin.SetStartCap(LineCapRound);spin.SetEndCap(LineCapRound);
            g.DrawArc(&spin,cx-12,cy-38,24.0f,24.0f,float(GetTickCount64()%1200)*0.3f,270.0f);
            ui::label(g,L"Bild wird geöffnet …",{r.x,cy+4,r.w,28},14,ui::muted,false,true);
        }else if(!app.error.empty()) {
            ui::icon(g,Icon::Image,cx-18,cy-82,ui::faint,36);
            ui::label(g,L"Dieses Bild lässt sich nicht öffnen",{r.x,cy-30,r.w,32},18,ui::text,true,true);
            ui::label(g,L"Die Datei ist beschädigt oder benötigt einen zusätzlichen Decoder.",{r.x+16,cy+8,r.w-32,48},12,ui::muted,false,true,true);
        }
    }
    float tx=std::max(8.0f,(l.canvas.w-452)/2),ty=l.canvas.y+l.canvas.h-58;
    ui::round(g,Color(80,0,0,0),{tx-1,ty+3,454,47},10);
    ui::round(g,ui::line,{tx-1,ty-1,454,46},9);ui::round(g,ui::surface,{tx,ty,452,44},8);
    for(float offset:{165.0f,297.0f,383.0f})ui::fill(g,ui::line,{tx+offset,ty+12,1,20});
    if(app.strip) {
        ui::fill(g,ui::surface,l.filmstrip);ui::fill(g,ui::line,{0,l.filmstrip.y,l.filmstrip.w,1});
        ui::label(g,std::to_wstring(app.index+1)+L" von "+std::to_wstring(app.files.size())+L" Bildern",{20,l.filmstrip.y+5,l.filmstrip.w-40,20},10,ui::muted,false,true);
    }
    if(app.info)drawInspector(g,l);
}
void paint(HWND window) {
    PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT bounds{};GetClientRect(window,&bounds);
    if(bounds.right<=0||bounds.bottom<=0){EndPaint(window,&ps);return;}
    HDC memory=CreateCompatibleDC(dc);HBITMAP buffer=CreateCompatibleBitmap(dc,bounds.right,bounds.bottom);auto previous=SelectObject(memory,buffer);
    {
        Graphics g(memory);float scale=app.dpi/96.0f;g.ScaleTransform(scale,scale);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        auto l=viewLayout();float w=l.width,h=l.height;ui::fill(g,ui::background,{0,0,w,h});ui::fill(g,ui::surface,{0,0,w,64});
        ui::brand(g,20,18);ui::label(g,L"Foto VLC",{60,14,95,34},17,ui::text,true);
        ui::round(g,ui::raised,{157,22,53,22},5);ui::label(g,L"v" FOTO_VERSION_WSTRING,{157,22,53,22},10,ui::muted,false,true);
        ui::fill(g,ui::line,{0,63,w,1});
        if(!app.files.empty()&&app.helpPage<0)ui::label(g,app.files[app.index].filename().wstring(),{235,16,w-513,32},13,ui::muted,false,true);
        if(app.helpPage>=0)drawHelp(g,l);else if(app.files.empty())drawWelcome(g,l);else drawViewer(g,l);
        ui::fill(g,ui::line,{0,h-28,w,1});
        std::wstring status=L"Bereit";
        if(!app.files.empty()&&app.helpPage<0) {
            status=std::to_wstring(app.index+1)+L" / "+std::to_wstring(app.files.size());
            if(app.picture)status+=L"   ·   "+std::to_wstring(app.picture->originalW)+L" × "+std::to_wstring(app.picture->originalH)+L" px   ·   "+app.fileSize;
            if(app.slideshow)status+=L"   ·   Diashow läuft";
        }
        ui::label(g,app.notice.empty()?status:app.notice,{20,h-27,w-295,26},10,app.notice.empty()?ui::muted:ui::accent);
        ui::label(g,L"Foto VLC " FOTO_VERSION_WSTRING L"   ·   F1 Hilfe",{w-230,h-27,210,26},10,ui::faint,false,true);
    }
    BitBlt(dc,0,0,bounds.right,bounds.bottom,memory,0,0,SRCCOPY);SelectObject(memory,previous);DeleteObject(buffer);DeleteDC(memory);EndPaint(window,&ps);
}
void choose(bool folder) {
    Com<IFileOpenDialog> dialog;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.out()))))return;
    DWORD flags=0;dialog->GetOptions(&flags);
    dialog->SetOptions(flags|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR|FOS_PATHMUSTEXIST|(folder?FOS_PICKFOLDERS:FOS_FILEMUSTEXIST));
    dialog->SetTitle(folder?L"Bilderordner öffnen":L"Bild öffnen");
    std::wstring patterns;
    if(!folder) {
        for(const auto& ext:foto::extensions()){if(!patterns.empty())patterns+=L';';patterns+=L"*."+std::wstring(ext.begin(),ext.end());}
        COMDLG_FILTERSPEC filters[]={{L"Bilddateien",patterns.c_str()},{L"Alle Dateien",L"*.*"}};dialog->SetFileTypes(2,filters);
    }
    fs::path last;
    if(!app.files.empty())last=app.files[app.index].parent_path();else if(!app.preferences.recent.empty())last=app.preferences.recent.front().parent_path();
    if(!last.empty()) {Com<IShellItem> item;if(SUCCEEDED(SHCreateItemFromParsingName(last.c_str(),nullptr,IID_PPV_ARGS(item.out()))))dialog->SetFolder(item);}
    if(SUCCEEDED(dialog->Show(app.window))) {
        Com<IShellItem> item;
        if(SUCCEEDED(dialog->GetResult(item.out()))) {PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){app.openPath(path);CoTaskMemFree(path);}}
    }
}
void copyPath() {
    if(app.files.empty()||!OpenClipboard(app.window))return;
    auto path=app.files[app.index].wstring();HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,(path.size()+1)*sizeof(wchar_t));
    if(data) {
        auto target=GlobalLock(data);
        if(target) {
            memcpy(target,path.c_str(),(path.size()+1)*sizeof(wchar_t));GlobalUnlock(data);
            EmptyClipboard();
            if(SetClipboardData(CF_UNICODETEXT,data)){app.notice=L"Dateipfad kopiert";SetTimer(app.window,3,2500,nullptr);}
            else GlobalFree(data);
        }else GlobalFree(data);
    }
    CloseClipboard();
}
void popup(int action) {
    HMENU menu=CreatePopupMenu();
    if(action==ZoomMenu) {
        for(int z:{25,50,100,200,400})AppendMenuW(menu,MF_STRING,400+z,(std::to_wstring(z)+L" %").c_str());
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,Fit,L"An Fenster anpassen\tF");
    }else {
        AppendMenuW(menu,MF_STRING,Open,L"Bild öffnen …\tStrg+O");AppendMenuW(menu,MF_STRING,OpenFolder,L"Ordner öffnen …\tStrg+Umschalt+O");
        if(!app.files.empty()) {
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,Reveal,L"Im Explorer zeigen");AppendMenuW(menu,MF_STRING,CopyPath,L"Dateipfad kopieren");
            AppendMenuW(menu,MF_STRING,Home,L"Zur Startseite");
        }
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,HelpKeys,L"Tastenkürzel\tF1");AppendMenuW(menu,MF_STRING,HelpFormats,L"Unterstützte Dateiformate");
        AppendMenuW(menu,MF_STRING,About,L"Über Foto VLC");
        if(!app.preferences.recent.empty()){AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,ClearRecent,L"Liste »Zuletzt geöffnet« leeren");}
    }
    RECT r{};GetWindowRect(app.controls[action].window,&r);
    int selected=int(TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTALIGN,r.right,r.bottom,0,app.window,nullptr));DestroyMenu(menu);
    if(selected>=425) {
        auto l=viewLayout();zoomAt((selected-400)/(app.zoom*100),l.image.x+l.image.w/2,l.image.y+l.image.h/2);
    }else if(selected)command(selected);
}
void command(int id) {
    if(id>=ThumbBase&&id<ThumbBase+20) {
        auto it=app.controls.find(id);if(it!=app.controls.end()&&it->second.item<app.files.size()){app.index=it->second.item;app.request();}return;
    }
    if(id>=RecentBase&&id<RecentBase+4){size_t i=size_t(id-RecentBase);if(i<app.preferences.recent.size())app.openPath(app.preferences.recent[i]);return;}
    bool loaded=bool(app.picture)&&!app.busy;
    switch(id) {
    case Open:case WelcomeOpen:choose(false);break;
    case OpenFolder:case WelcomeFolder:choose(true);break;
    case Prev:case Next:if(app.files.size()>1){app.index=foto::neighbor(app.index,id==Next?1:-1,app.files.size());app.request();}return;
    case Fit:if(loaded){app.fit=true;app.panX=app.panY=0;}break;
    case Actual:if(loaded){app.fit=false;app.zoom=1;app.panX=app.panY=0;}break;
    case ZoomIn:case ZoomOut:if(loaded){auto l=viewLayout();zoomAt(id==ZoomIn?1.25:0.8,l.image.x+l.image.w/2,l.image.y+l.image.h/2);}return;
    case Rotate:if(loaded){app.rotation=(app.rotation+1)%4;app.panX=app.panY=0;}break;
    case Info:app.info=!app.info;app.saveSettings();break;
    case Strip:app.strip=!app.strip;app.saveSettings();break;
    case Home:
        if(app.slideshow)command(Slide);
        ++app.serial;app.resetFolder();app.files.clear();app.picture.reset();app.error.clear();app.busy=false;app.helpPage=-1;
        {std::lock_guard<std::mutex> lock(app.mutex);app.queued.reset();}
        SetWindowTextW(app.window,AppTitle);SetFocus(app.window);break;
    case Full:
        if(!app.fullscreen) {
            app.restoreStyle=GetWindowLongW(app.window,GWL_STYLE);GetWindowRect(app.window,&app.restore);
            MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);GetMonitorInfoW(MonitorFromWindow(app.window,MONITOR_DEFAULTTONEAREST),&monitor);
            SetWindowLongW(app.window,GWL_STYLE,app.restoreStyle&~WS_OVERLAPPEDWINDOW);
            SetWindowPos(app.window,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_FRAMECHANGED);
        }else {
            SetWindowLongW(app.window,GWL_STYLE,app.restoreStyle);auto r=app.restore;
            SetWindowPos(app.window,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED|SWP_NOZORDER);
        }
        app.fullscreen=!app.fullscreen;break;
    case Slide:
        if(app.files.size()<2)return;
        app.slideshow=!app.slideshow;if(app.slideshow)SetTimer(app.window,1,4000,nullptr);else KillTimer(app.window,1);break;
    case About:case HelpKeys:case Formats:case HelpFormats:
        if(app.slideshow)command(Slide);
        app.helpPage=id==About?0:id==HelpKeys?1:2;SetFocus(app.window);break;
    case CloseHelp:app.helpPage=-1;SetFocus(app.window);break;
    case More:case ZoomMenu:popup(id);return;
    case Reveal:
        if(!app.files.empty()) {
            PIDLIST_ABSOLUTE item=ILCreateFromPathW(app.files[app.index].c_str());
            if(item){SHOpenFolderAndSelectItems(item,0,nullptr,0);ILFree(item);}
        }break;
    case CopyPath:copyPath();break;
    case ClearRecent:app.preferences.recent.clear();app.saveSettings();break;
    case ReleasePage:ShellExecuteW(app.window,L"open",app.helpPage==2?L"https://github.com/Lyvt-Dev/Foto-VLC/blob/main/docs/formats.md":L"https://github.com/Lyvt-Dev/Foto-VLC/releases",nullptr,nullptr,SW_SHOWNORMAL);break;
    case SourcePage:ShellExecuteW(app.window,L"open",L"https://github.com/Lyvt-Dev/Foto-VLC",nullptr,nullptr,SW_SHOWNORMAL);break;
    case ErrorDetails:MessageBoxW(app.window,app.error.c_str(),L"Warum kann dieses Bild nicht geöffnet werden?",MB_OK|MB_ICONINFORMATION);break;
    }
    refresh();
}
LRESULT CALLBACK wndProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    switch(message) {
    case WM_CREATE:
        app.window=window;app.dpi=GetDpiForWindow(window);DragAcceptFiles(window,TRUE);
        app.tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        SendMessageW(app.tooltip,TTM_SETMAXTIPWIDTH,0,420);SendMessageW(app.tooltip,TTM_SETDELAYTIME,TTDT_INITIAL,450);
        app.begin();layoutControls();return 0;
    case WM_PAINT:paint(window);return 0;
    case WM_DRAWITEM:drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){command(LOWORD(wp));return 0;}break;
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:refresh();return 0;
    case WM_GETMINMAXINFO: {
        auto info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={LONG(780*app.dpi/96),LONG(560*app.dpi/96)};return 0;
    }
    case WM_DPICHANGED: {
        app.dpi=HIWORD(wp);auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(window,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);refresh();return 0;
    }
    case Done: {
        std::optional<Completion> result;
        {std::lock_guard<std::mutex> lock(app.mutex);result=std::move(app.completed);app.completed.reset();}
        if(result&&result->serial==app.serial) {
            app.picture=std::move(result->decoded.image);app.error=std::move(result->decoded.error);app.busy=false;KillTimer(window,2);
            if(app.picture&&!app.files.empty()){app.preferences.remember(app.files[app.index]);app.saveSettings();}
            refresh();
        }return 0;
    }
    case ThumbDone: {
        std::deque<ThumbResult> results;
        {std::lock_guard<std::mutex> lock(app.mutex);results.swap(app.thumbResults);}
        for(auto& result:results)if(result.folder==app.folderSerial)app.thumbs[result.path]=std::move(result.picture);
        for(auto& entry:app.controls)if(entry.first>=ThumbBase&&entry.second.visible)InvalidateRect(entry.second.window,nullptr,FALSE);
        return 0;
    }
    case WM_DROPFILES: {
        auto drop=reinterpret_cast<HDROP>(wp);UINT length=DragQueryFileW(drop,0,nullptr,0);std::vector<wchar_t> path(length+1);
        if(length&&DragQueryFileW(drop,0,path.data(),length+1))app.openPath(path.data());
        DragFinish(drop);return 0;
    }
    case WM_LBUTTONDOWN: {
        SetFocus(window);POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};float scale=app.dpi/96.0f;auto r=viewLayout().image;
        if(app.picture&&app.helpPage<0&&RectF(r.x,r.y,r.w,r.h).Contains(p.x/scale,p.y/scale)){app.dragging=true;app.drag=p;SetCapture(window);}return 0;
    }
    case WM_MOUSEMOVE:
        if(app.dragging) {
            POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};double scale=app.dpi/96.0;
            app.panX+=(p.x-app.drag.x)/scale;app.panY+=(p.y-app.drag.y)/scale;app.drag=p;updateZoom();InvalidateRect(window,nullptr,FALSE);
        }return 0;
    case WM_LBUTTONUP:app.dragging=false;ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:app.dragging=false;return 0;
    case WM_LBUTTONDBLCLK:if(app.picture&&app.helpPage<0)command(app.fit?Actual:Fit);return 0;
    case WM_MOUSEWHEEL:
        if(app.picture&&app.helpPage<0) {
            POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(window,&p);float scale=app.dpi/96.0f;
            auto r=viewLayout().image;
            if(RectF(r.x,r.y,r.w,r.h).Contains(p.x/scale,p.y/scale))zoomAt(std::pow(1.25,GET_WHEEL_DELTA_WPARAM(wp)/120.0),p.x/scale,p.y/scale);
        }return 0;
    case WM_KEYDOWN:if(shortcut(UINT(wp)))return 0;break;
    case WM_TIMER:
        if(wp==1&&!app.busy&&app.files.size()>1&&app.helpPage<0)command(Next);
        if(wp==2&&app.busy)InvalidateRect(window,nullptr,FALSE);
        if(wp==3){app.notice.clear();KillTimer(window,3);InvalidateRect(window,nullptr,FALSE);}
        return 0;
    case WM_CLOSE:
        if(!app.fullscreen&&!IsIconic(window)&&!IsZoomed(window)) {
            RECT r{};GetWindowRect(window,&r);app.preferences.width=int((r.right-r.left)*96/app.dpi);app.preferences.height=int((r.bottom-r.top)*96/app.dpi);
        }
        app.saveSettings();DestroyWindow(window);return 0;
    case WM_DESTROY:
        KillTimer(window,1);KillTimer(window,2);KillTimer(window,3);app.shutdown();if(app.tooltip)DestroyWindow(app.tooltip);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}

void dispatchMessage(MSG& message) {
        if(message.message==WM_KEYDOWN&&(message.hwnd==app.window||IsChild(app.window,message.hwnd))) {
            if(message.wParam==VK_RETURN) {
                int id=GetDlgCtrlID(message.hwnd);
                if(app.controls.count(id)&&app.controls[id].visible&&app.controls[id].enabled)command(id);
                return;
            }
            if(message.wParam!=VK_TAB&&(message.wParam!=VK_SPACE||message.hwnd==app.window)&&shortcut(UINT(message.wParam)))return;
        }
        if(!IsDialogMessageW(app.window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
}

// Exercise the real Windows decoder, not just a file extension table.
int selfTest(const fs::path& report) {
    app.testing=true;
    std::ofstream log(report);int passed=0;
    auto check=[&](bool ok,const char* name){log<<(ok?"PASS ":"FAIL ")<<name<<"\n";if(ok)++passed;return ok;};
    TempDirectory temp;if(temp.path.empty())return 2;
    Com<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.out()))))return 3;
    bool success=true;
    const std::pair<const wchar_t*,GUID> formats[]={{L"png",GUID_ContainerFormatPng},{L"jpg",GUID_ContainerFormatJpeg},{L"bmp",GUID_ContainerFormatBmp},{L"tiff",GUID_ContainerFormatTiff}};
    for(const auto& format:formats) {
        auto path=temp.path/(L"Bilder ü 日本語."+std::wstring(format.first));
        Com<IWICStream> stream;Com<IWICBitmapEncoder> encoder;Com<IWICBitmapFrameEncode> frame;Com<IPropertyBag2> properties;
        HRESULT hr=factory->CreateStream(stream.out());
        if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);
        if(SUCCEEDED(hr))hr=factory->CreateEncoder(format.second,nullptr,encoder.out());
        if(SUCCEEDED(hr))hr=encoder->Initialize(stream,WICBitmapEncoderNoCache);
        if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(frame.out(),properties.out());
        if(SUCCEEDED(hr))hr=frame->Initialize(properties);
        if(SUCCEEDED(hr))hr=frame->SetSize(2,2);
        WICPixelFormatGUID pixels=GUID_WICPixelFormat24bppBGR;
        if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&pixels);
        BYTE data[]={0,0,255, 0,0,255, 0,0,255, 0,0,255};
        if(SUCCEEDED(hr))hr=frame->WritePixels(2,6,sizeof(data),data);
        if(SUCCEEDED(hr))hr=frame->Commit();
        if(SUCCEEDED(hr))hr=encoder->Commit();
        // Close the stream and encoder before opening the result.
        frame.out();encoder.out();stream.out();
        auto decoded=wicLoad(path);
        success &=check(SUCCEEDED(hr)&&decoded.image && decoded.image->width==2 && decoded.image->height==2,"WIC fixture + Unicode path + dimensions");
        success &=check(decoded.image && decoded.image->pixels[2]>240 && decoded.image->pixels[0]<15,"Decoded red pixels");
        if(decoded.image) {
            auto& p=*decoded.image;Bitmap source(p.width,p.height,p.width*4,PixelFormat32bppPARGB,p.pixels.data());
            Bitmap destination(16,16,PixelFormat32bppARGB);Graphics g(&destination);
            g.Clear(Color(255,0,0,0));g.DrawImage(&source,Rect(0,0,16,16));Color pixel;destination.GetPixel(8,8,&pixel);
            success &=check(pixel.GetR()>240 && pixel.GetB()<15,"GDI+ render");
        }
    }
    fs::copy_file(temp.path/L"Bilder ü 日本語.png",temp.path/L"Bild 2.png");
    fs::copy_file(temp.path/L"Bilder ü 日本語.png",temp.path/L"Bild 10.png");
    auto invalid=temp.path/L"broken.png";{std::ofstream f(invalid);f<<"not an image";}
    success &=check(!wicLoad(invalid).image,"Reject corrupt image");
    success &=check(!wicLoad(temp.path/L"missing.png").image,"Reject missing file");
    success &=check(foto::recognized("UPPER.PNG") && !foto::recognized("image.png.exe"),"Extension registry");
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=wndProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=AppClass;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.style=CS_DBLCLKS;
    if(RegisterClassExW(&wc)) {
        HWND window=CreateWindowExW(0,AppClass,L"Foto VLC · UI-Test",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,1180,800,nullptr,nullptr,wc.hInstance,nullptr);
        success &=check(window!=nullptr,"Create desktop window");
        if(window) {
            ShowWindow(window,SW_SHOW);
            app.openPath(temp.path/L"Bilder ü 日本語.png");
            auto pump=[&]{
                DWORD start=GetTickCount();
                while(app.busy && GetTickCount()-start<5000) {
                    MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){dispatchMessage(message);}
                    Sleep(10);
                }
                UpdateWindow(window);
            };
            pump();
            success &=check(app.picture && !app.busy && app.error.empty(),"Asynchronous desktop image load");
            success &=check(app.files.size()>=5,"Folder image discovery");
            auto two=std::find(app.files.begin(),app.files.end(),temp.path/L"Bild 2.png");
            auto ten=std::find(app.files.begin(),app.files.end(),temp.path/L"Bild 10.png");
            success &=check(two!=app.files.end()&&ten!=app.files.end()&&two<ten,"Natural filename ordering");
            auto selected=app.index;command(Next);pump();command(Prev);pump();
            success &=check(app.index==selected && app.picture,"Next and previous navigation");
            command(Actual);command(ZoomIn);command(Rotate);
            success &=check(!app.fit && app.zoom==1.25 && app.rotation==1,"Zoom and rotation controls");
            command(Fit);success &=check(app.fit && app.panX==0 && app.panY==0,"Fit view");
            command(Full);success &=check(app.fullscreen,"Enter fullscreen");command(Full);
            success &=check(!app.fullscreen,"Exit fullscreen");
            command(Slide);success &=check(app.slideshow,"Slideshow timer start");command(Slide);
            success &=check(!app.slideshow,"Slideshow timer stop");
            UpdateWindow(window);success &=check(IsWindowVisible(app.controls[Fit].window),"Native desktop toolbar");
            // Buttons expose their accessible names and receive real native click messages.
            wchar_t accessibleName[128]{};GetWindowTextW(app.controls[Rotate].window,accessibleName,128);
            success &=check(std::wstring(accessibleName)==L"Drehen","Native button accessible label");
            int before=app.rotation;SendMessageW(app.controls[Rotate].window,BM_CLICK,0,0);
            success &=check(app.rotation==(before+1)%4,"Native button click dispatch");
            bool previousInfo=app.info;command(Info);
            success &=check(app.info!=previousInfo && IsWindowVisible(app.controls[Reveal].window)==app.info,"Details panel toggle");
            command(Info);bool previousStrip=app.strip;command(Strip);
            success &=check(app.strip!=previousStrip&&!IsWindowVisible(app.controls[ThumbBase].window),"Hide filmstrip");
            command(Strip);
            DWORD thumbStart=GetTickCount();
            while(GetTickCount()-thumbStart<5000 && app.thumbs.empty()) {
                MSG event;while(PeekMessageW(&event,nullptr,0,0,PM_REMOVE)){TranslateMessage(&event);DispatchMessageW(&event);}Sleep(10);
            }
            bool hasPreview=false;for(const auto& entry:app.thumbs)if(entry.second)hasPreview=true;
            success &=check(hasPreview,"Real asynchronous image thumbnails");
            success &=check(!app.preferences.recent.empty() && app.preferences.recent.front()==app.files[app.index],"Recent successful image");
            auto preferencesFile=temp.path/L"settings.ini";Preferences saved;saved.details=true;saved.strip=false;saved.width=1110;
            saved.remember(temp.path/L"Bilder ü 日本語.png");saved.write(preferencesFile);Preferences restored;restored.read(preferencesFile);
            success &=check(restored.details&&!restored.strip&&restored.width==1110&&restored.recent==saved.recent,"Preferences and Unicode recent paths persist");
            command(About);success &=check(app.helpPage==0&&IsWindowVisible(app.controls[ReleasePage].window),"About and version page");
            command(HelpKeys);success &=check(app.helpPage==1,"Keyboard help page");
            MSG escape{};escape.hwnd=app.controls[HelpKeys].window;escape.message=WM_KEYDOWN;escape.wParam=VK_ESCAPE;dispatchMessage(escape);
            success &=check(app.helpPage==-1,"Escape closes help from a focused native button");
            before=app.rotation;MSG enter{};enter.hwnd=app.controls[Rotate].window;enter.message=WM_KEYDOWN;enter.wParam=VK_RETURN;dispatchMessage(enter);
            success &=check(app.rotation==(before+1)%4,"Enter activates focused native button");
            success &=check(app.helpPage==-1&&app.picture,"Return from help preserves photo");
            for(auto size:{SIZE{780,560},SIZE{1180,800},SIZE{1600,1000}}) {
                SetWindowPos(window,nullptr,0,0,size.cx,size.cy,SWP_NOZORDER);UpdateWindow(window);
                RECT client{};GetClientRect(window,&client);bool fits=true;float scale=app.dpi/96.0f;
                for(const auto& entry:app.controls)if(entry.second.visible) {
                    const auto& box=entry.second.box;
                    if(box.x<0||box.y<0||(box.x+box.w)*scale>client.right+1||(box.y+box.h)*scale>client.bottom+1)fits=false;
                }
                success &=check(fits,"Responsive controls stay inside window");
            }
            command(Home);success &=check(app.files.empty()&&!app.picture&&IsWindowVisible(app.controls[WelcomeOpen].window),"Start page with recent images");
            DestroyWindow(window);MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){}
        }
        UnregisterClassW(AppClass,wc.hInstance);
    }else success &=check(false,"Register desktop window");
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);DWORD unused=0;
    DWORD bytes=GetFileVersionInfoSizeW(executable,&unused);std::vector<BYTE> version(bytes);bool correctVersion=false;
    if(bytes&&GetFileVersionInfoW(executable,0,bytes,version.data())) {
        LPVOID value=nullptr;UINT length=0;
        if(VerQueryValueW(version.data(),L"\\StringFileInfo\\040904B0\\ProductVersion",&value,&length))correctVersion=std::wstring(static_cast<wchar_t*>(value))==FOTO_VERSION_WSTRING;
    }
    success &=check(correctVersion,"Executable metadata matches visible app version");
    log<<passed<<" checks passed\n";return success?0:1;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    GdiplusStartupInput startup;ULONG_PTR token=0;
    if(GdiplusStartup(&token,&startup,nullptr)!=Ok){if(SUCCEEDED(co))CoUninitialize();return 1;}
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argc>1&&std::wstring(args[1])==L"--self-test") {
        int result=selfTest(argc>2?fs::path(args[2]):fs::path(L"self-test.txt"));LocalFree(args);GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return result;
    }
    app.initSettings();
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_DBLCLKS;wc.lpfnWndProc=wndProc;wc.hInstance=instance;wc.lpszClassName=AppClass;
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(2));wc.hIconSm=wc.hIcon;
    if(!RegisterClassExW(&wc)){LocalFree(args);GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return 1;}
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    UINT dpi=GetDpiForSystem();int width=std::min(int(app.preferences.width*dpi/96),int(work.right-work.left)),height=std::min(int(app.preferences.height*dpi/96),int(work.bottom-work.top));
    HWND window=CreateWindowExW(0,AppClass,AppTitle,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,nullptr,nullptr,instance,nullptr);
    if(!window){LocalFree(args);GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return 1;}
    BOOL dark=TRUE;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
    COLORREF caption=RGB(25,28,32);DwmSetWindowAttribute(window,35,&caption,sizeof(caption));
    ShowWindow(window,show);UpdateWindow(window);
    if(argc>1)app.openPath(args[1]);
    LocalFree(args);
    MSG message{};int status;
    while((status=GetMessageW(&message,nullptr,0,0))>0) {
        dispatchMessage(message);
    }
    GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return status<0?1:int(message.wParam);
}
