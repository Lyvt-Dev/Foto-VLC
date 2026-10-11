#include "image.hpp"
#include "export.hpp"
#include "updater.hpp"
#include "motion.hpp"
#include <commdlg.h>
#include "core.hpp"
#include "layout.hpp"
#include "version.h"
#include "i18n.hpp"
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
#include <chrono>
using namespace Gdiplus;
constexpr UINT Done=WM_APP+1,ThumbDone=WM_APP+2,ExportDone=WM_APP+3,UpdateDone=WM_APP+4;
constexpr wchar_t AppClass[]=L"PAFPhotos.Main";
constexpr wchar_t AppTitle[]=L"PAF-Photos · " FOTO_VERSION_WSTRING;
enum Action {
    Open=1,OpenFolder,Prev,Next,Fit,Actual,ZoomOut,ZoomIn,Rotate,Full,Info,Formats,Slide,
    Home,Strip,More,About,HelpKeys,HelpFormats,CloseHelp,Reveal,CopyPath,ClearRecent,
    ReleasePage,SourcePage,ErrorDetails,WelcomeOpen,WelcomeFolder,ZoomMenu,
    NavToggle=30,Gallery,Favorites,Settings,ImportFolder,Favorite,Language,Theme,DefaultZoom,TileInfo,Sort,GridSize,SelectMode,ClearSelection,RemoveFolder,Minimize,Maximize,Exit,
    Export=48,Mirror,CopyRaster,ResetView,Accent,Radius,Animations,Recursive,Quality,
    AutoUpdates=57,CheckUpdates,RevealSaved,
    Search=60,CloseExport,OpenSaved,GalleryBase=1000,RecentBase=100,ThumbBase=200
};
enum class Icon {None,Image,Folder,Left,Right,Fit,Actual,Minus,Plus,Rotate,Full,Info,Strip,Play,Pause,More,Home,Close,Help,External,Copy,Menu,Heart,Settings,Search,Sort,Grid,Check,Computer,Window,Mirror,Convert};
struct RenderBuffer {
    HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ previous=nullptr;int width=0,height=0;
    RenderBuffer()=default;RenderBuffer(const RenderBuffer&)=delete;RenderBuffer& operator=(const RenderBuffer&)=delete;
    ~RenderBuffer(){reset();}
    void reset(){if(dc){if(previous)SelectObject(dc,previous);if(bitmap)DeleteObject(bitmap);DeleteDC(dc);}dc=nullptr;bitmap=nullptr;previous=nullptr;width=height=0;}
    HDC prepare(HDC source,int w,int h){if(dc&&width==w&&height==h)return dc;reset();if(w<=0||h<=0)return nullptr;
        dc=CreateCompatibleDC(source);if(!dc)return nullptr;bitmap=CreateCompatibleBitmap(source,w,h);if(!bitmap){reset();return nullptr;}
        previous=SelectObject(dc,bitmap);width=w;height=h;return dc;}
};
struct Control {
    HWND window=nullptr;
    int id=0;
    foto::Box box;
    std::wstring caption,tip,accessible;
    RenderBuffer buffer;RECT pixelBounds{},clipBounds{};bool clipped=false,wantsClip=false;
    Icon icon=Icon::None;
    bool visible=false,enabled=true,active=false,primary=false;
    float hoverProgress=0;
    size_t item=0;
};
struct Preferences {
    bool details=false,strip=true,tileInfo=true,actual=false,animations=true,recursive=false,autoUpdates=true;
    int radius=8,quality=95;
    DWORD accent=RGB(76,194,255);
    int language=0,theme=0;
    std::vector<fs::path> folders,favorites;
    int width=1280,height=860;
    std::vector<fs::path> recent;
    void read(const fs::path& file) {
        if(file.empty())return;
        details=GetPrivateProfileIntW(L"View",L"Details",0,file.c_str())!=0;
        strip=GetPrivateProfileIntW(L"View",L"Filmstrip",1,file.c_str())!=0;
        width=std::clamp(int(GetPrivateProfileIntW(L"View",L"Width",1280,file.c_str())),780,2400);
        height=std::clamp(int(GetPrivateProfileIntW(L"View",L"Height",860,file.c_str())),560,1600);
        language=GetPrivateProfileIntW(L"View",L"Language",0,file.c_str());
        theme=GetPrivateProfileIntW(L"View",L"Theme",0,file.c_str());
        accent=GetPrivateProfileIntW(L"View",L"Accent",RGB(76,194,255),file.c_str())&0xffffff;
        radius=std::clamp(int(GetPrivateProfileIntW(L"View",L"Radius",8,file.c_str())),0,16);
        quality=std::clamp(int(GetPrivateProfileIntW(L"View",L"Quality",95,file.c_str())),1,100);
        animations=GetPrivateProfileIntW(L"View",L"Animations",1,file.c_str())!=0;
        autoUpdates=GetPrivateProfileIntW(L"View",L"AutoUpdates",1,file.c_str())!=0;
        recursive=GetPrivateProfileIntW(L"View",L"Recursive",0,file.c_str())!=0;
        tileInfo=GetPrivateProfileIntW(L"View",L"TileInfo",1,file.c_str())!=0;
        actual=GetPrivateProfileIntW(L"View",L"Actual",0,file.c_str())!=0;
        for(auto section:{L"Folders",L"Favorites"}) {
            auto& list=std::wstring(section)==L"Folders"?folders:favorites;list.clear();
            for(int i=0;i<1000;++i){wchar_t value[32768]{};GetPrivateProfileStringW(section,std::to_wstring(i).c_str(),L"",value,32768,file.c_str());if(!*value)break;list.emplace_back(value);}
        }
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
               <<L"\r\nLanguage="<<language<<L"\r\nTheme="<<theme<<L"\r\nTileInfo="<<tileInfo<<L"\r\nActual="<<actual
               <<L"\r\nAccent="<<accent<<L"\r\nRadius="<<radius<<L"\r\nQuality="<<quality<<L"\r\nAnimations="<<animations<<L"\r\nRecursive="<<recursive
               <<L"\r\nAutoUpdates="<<autoUpdates
               <<L"\r\nWidth="<<width<<L"\r\nHeight="<<height<<L"\r\n[Recent]\r\n";
        for(size_t i=0;i<recent.size()&&i<4;++i)content<<i<<L"="<<recent[i].wstring()<<L"\r\n";
        for(auto section:{L"Folders",L"Favorites"}) {
            content<<L"["<<section<<L"]\r\n";const auto& list=std::wstring(section)==L"Folders"?folders:favorites;
            for(size_t i=0;i<list.size()&&i<1000;++i)content<<i<<L"="<<list[i].wstring()<<L"\r\n";
        }
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
struct ExportJob{std::shared_ptr<Picture> picture;fs::path original,target;ExportFormat format;int rotation;bool mirrored;unsigned quality;};
struct ExportResult{fs::path target;bool saved;};
struct Completion {unsigned serial;Decode decoded;};
struct ThumbJob {fs::path path;unsigned folder;};
struct ThumbResult {fs::path path;unsigned folder;std::shared_ptr<Picture> picture;};
void refresh();
void command(int id);
void layoutControls();
void drawButton(const DRAWITEMSTRUCT& item);
LRESULT CALLBACK wndProc(HWND,UINT,WPARAM,LPARAM);

std::shared_ptr<Picture> thumbnail(const fs::path& path) {
    auto decoded=wicLoad(path,400);
    if(decoded.image)return decoded.image;
    std::error_code ec;
    auto size=fs::file_size(path,ec);if(size>512ull*1024*1024 || ec)return {};
    if(size<=16u*1024*1024)decoded=embeddedLoad(path);
    if(!decoded.image&&!magickPath().empty())decoded=load(path,400);
    if(!decoded.image)return {};
    auto& original=*decoded.image;
    double scale=std::min({1.0,400.0/original.width,400.0/original.height});
    auto result=std::make_shared<Picture>();result->width=std::max(1u,UINT(original.width*scale));result->height=std::max(1u,UINT(original.height*scale));
    result->pixels.resize(size_t(result->width)*result->height*4);
    Bitmap source(original.width,original.height,original.width*4,PixelFormat32bppPARGB,original.pixels.data());
    Bitmap destination(result->width,result->height,result->width*4,PixelFormat32bppPARGB,result->pixels.data());
    Graphics graphics(&destination);graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics.DrawImage(&source,Rect(0,0,result->width,result->height));return result;
}
struct App {
    HWND window=nullptr,tooltip=nullptr,search=nullptr;
    HFONT searchFont=nullptr;
    bool gallery=true,favoritesView=false,settings=false,navExpanded=false,selectMode=false,reverseSort=false;
    paf::ScrollMotion galleryScroll,settingsScroll;
    double galleryMaxScroll=0;int tileSize=210;
    std::chrono::steady_clock::time_point motionTime{};bool scrollAnimating=false;float scrollGrab=0;RenderBuffer buffer;
    bool scrollDragging=false;
    std::vector<fs::path> library,filtered;
    std::set<fs::path> selected;
    std::wstring query;
    Preferences preferences;
    fs::path settingsPath,lastExport;
    std::vector<fs::path> files;
    size_t index=0;
    std::shared_ptr<Picture> picture;
    std::wstring error,fileSize,fileDate,notice;
    bool busy=false,info=false,strip=true,fullscreen=false,slideshow=false,fit=true,dragging=false,testing=false;
    UpdateService updates;
    bool installed=false,installUpdateNow=false,restartAfterUpdate=false,exportNotice=false;
    fs::path executableDirectory;
    bool mirrored=false,exporting=false;
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
    std::optional<ExportJob> queuedExport;
    std::optional<ExportResult> exported;
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
            std::error_code ec;auto oldDirectory=fs::path(local)/L"PhotoVLC";if(!fs::exists(oldDirectory/L"settings.ini",ec))oldDirectory=fs::path(local)/L"FotoVLC";auto directory=fs::path(local)/L"PAFPhotos";CoTaskMemFree(local);
            ec.clear();fs::create_directories(directory,ec);
            if(!ec){settingsPath=directory/L"settings.ini";if(!fs::exists(settingsPath,ec)&&fs::exists(oldDirectory/L"settings.ini",ec))fs::copy_file(oldDirectory/L"settings.ini",settingsPath,fs::copy_options::skip_existing,ec);}
        }
        preferences.read(settingsPath);info=preferences.details;strip=preferences.strip;
        applyLanguage();rebuildLibrary();
    }
    void applyLanguage() {foto::english=preferences.language==2||(preferences.language==0&&PRIMARYLANGID(GetUserDefaultUILanguage())!=LANG_GERMAN);}
    void filterLibrary() {
        filtered.clear();auto needle=query;CharLowerBuffW(needle.data(),DWORD(needle.size()));
        for(const auto& path:library){if(favoritesView&&std::find(preferences.favorites.begin(),preferences.favorites.end(),path)==preferences.favorites.end())continue;
            auto name=path.filename().wstring();CharLowerBuffW(name.data(),DWORD(name.size()));if(needle.empty()||name.find(needle)!=std::wstring::npos)filtered.push_back(path);}
        if(reverseSort)std::reverse(filtered.begin(),filtered.end());
        galleryScroll.reset();
        for(auto it=selected.begin();it!=selected.end();)if(std::find(filtered.begin(),filtered.end(),*it)==filtered.end())it=selected.erase(it);else ++it;
    }
    void rebuildLibrary() {
        std::set<fs::path> unique;std::error_code ec;
        for(const auto& folder:preferences.folders){
            auto scan=[&](auto iterator){decltype(iterator) end;for(;!ec&&iterator!=end&&unique.size()<10000;iterator.increment(ec)){
                std::error_code error;if(iterator->is_regular_file(error)&&known(iterator->path()))unique.insert(iterator->path());}ec.clear();};
            if(preferences.recursive)scan(fs::recursive_directory_iterator(folder,fs::directory_options::skip_permission_denied,ec));
            else scan(fs::directory_iterator(folder,fs::directory_options::skip_permission_denied,ec));
        }
        for(const auto& path:preferences.recent)if(fs::is_regular_file(path,ec))unique.insert(path);
        for(const auto& path:preferences.favorites)if(fs::is_regular_file(path,ec))unique.insert(path);
        library.assign(unique.begin(),unique.end());std::sort(library.begin(),library.end(),[](const auto& a,const auto& b){return StrCmpLogicalW(a.filename().c_str(),b.filename().c_str())<0;});filterLibrary();
    }
    void saveSettings() {
        if(testing)return;
        preferences.details=info;preferences.strip=strip;preferences.write(settingsPath);
    }
    void begin() {
        worker=std::thread([this]{
            HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            for(;;) {
                std::optional<Job> task;std::optional<ThumbJob> preview;std::optional<ExportJob> conversion;
                {std::unique_lock<std::mutex> lock(mutex);
                 cv.wait(lock,[this]{return stopping||queued.has_value()||queuedExport.has_value()||!thumbQueue.empty();});
                 if(stopping)break;
                 if(queued){task=*queued;queued.reset();}
                 else if(queuedExport){conversion=std::move(queuedExport);queuedExport.reset();}
                 else{preview=thumbQueue.front();thumbQueue.pop_front();}}
                if(task) {
                    Decode result;
                    try {result=load(task->path);}catch(const std::exception&){result.error=tr(L"Die Datei konnte nicht vollständig gelesen werden.");}
                    {std::lock_guard<std::mutex> lock(mutex);if(stopping)break;completed=Completion{task->serial,std::move(result)};}
                    PostMessageW(window,Done,0,0);
                } else if(conversion){
                    bool saved=false;try{saved=exportPicture(*conversion->picture,conversion->original,conversion->target,conversion->format,conversion->rotation,conversion->mirrored,conversion->quality);}catch(const std::exception&){}
                    {std::lock_guard<std::mutex> lock(mutex);if(stopping)break;exported=ExportResult{conversion->target,saved};}
                    PostMessageW(window,ExportDone,0,0);
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
    bool exportCopy(const fs::path& target,ExportFormat format){
        if(exporting||!picture||busy||files.empty())return false;
        exporting=true;exportNotice=false;notice=tr(L"Bild wird konvertiert …");KillTimer(window,3);
        {std::lock_guard<std::mutex> lock(mutex);queuedExport=ExportJob{picture,files[index],target,format,rotation,mirrored,unsigned(preferences.quality)};}
        cv.notify_one();return true;
    }
    void shutdown() {
        updates.stop();
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;queued.reset();queuedExport.reset();thumbQueue.clear();}
        cv.notify_one();if(worker.joinable())worker.join();
    }
    void resetFolder() {
        ++folderSerial;thumbs.clear();requestedThumbs.clear();
        std::lock_guard<std::mutex> lock(mutex);thumbQueue.clear();thumbResults.clear();
    }
    void request() {
        if(files.empty())return;
        exportNotice=false;++serial;busy=true;error.clear();picture.reset();gallery=false;settings=false;fit=!preferences.actual;zoom=1;rotation=0;mirrored=false;panX=panY=0;helpPage=-1;
        SetWindowTextW(window,(files[index].filename().wstring()+L" — "+AppTitle).c_str());
        std::error_code ec;auto bytes=fs::file_size(files[index],ec);
        std::wostringstream size;
        if(bytes<1024)size<<bytes<<L" Byte";
        else if(bytes<1048576)size<<std::fixed<<std::setprecision(0)<<bytes/1024.0<<L" KB";
        else size<<std::fixed<<std::setprecision(1)<<bytes/1048576.0<<L" MB";
        fileSize=ec?tr(L"Nicht verfügbar"):size.str();fileDate=tr(L"Nicht verfügbar");
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
            MessageBoxW(window,tr(L"Die Datei ist nicht mehr an diesem Ort. Öffne sie bitte erneut über »Öffnen«."),tr(L"Datei nicht gefunden"),MB_OK|MB_ICONINFORMATION);return;
        }
        auto root=directory?path:path.parent_path();std::vector<fs::path> found;
        fs::directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
        for(;!ec&&it!=end;it.increment(ec)) {
            std::error_code e;if(it->is_regular_file(e)&&known(it->path()))found.push_back(it->path());
        }
        if(!directory&&std::find(found.begin(),found.end(),path)==found.end())found.push_back(path);
        std::sort(found.begin(),found.end(),[](const auto& a,const auto& b){return StrCmpLogicalW(a.filename().c_str(),b.filename().c_str())<0;});
        if(found.empty()) {
            MessageBoxW(window,tr(L"In diesem Ordner wurden keine Bilddateien gefunden. Wähle einen anderen Ordner aus."),tr(L"Keine Bilder im Ordner"),MB_OK|MB_ICONINFORMATION);return;
        }
        resetFolder();files=std::move(found);
        auto selected=std::find(files.begin(),files.end(),path);index=selected==files.end()?0:size_t(selected-files.begin());request();
    }
};
App app;
void checkUpdates(bool manual=false){
    if(app.testing||app.settingsPath.empty())return;
    auto state=app.updates.snapshot();
    if(manual&&state.phase==UpdatePhase::Available&&!app.installed){ShellExecuteW(app.window,L"open",L"https://github.com/Lyvt-Dev/PAF-Photos/releases/latest",nullptr,nullptr,SW_SHOWNORMAL);return;}
    if(manual&&state.phase==UpdatePhase::Ready){
        if(!app.installed){ShellExecuteW(app.window,L"open",state.installer.c_str(),nullptr,nullptr,SW_SHOWNORMAL);return;}
        if(app.exporting){app.notice=tr(L"Bitte warte, bis die Konvertierung abgeschlossen ist.");refresh();return;}
        app.installUpdateNow=true;app.restartAfterUpdate=true;PostMessageW(app.window,WM_CLOSE,0,0);return;
    }
    auto cache=app.settingsPath.parent_path()/L"updates";
    app.updates.start(app.window,UpdateDone,FOTO_VERSION_STRING,cache,app.installed&&(app.preferences.autoUpdates||manual));refresh();
}
const wchar_t* updateStatus(){
    static std::wstring description;auto state=app.updates.snapshot();
    switch(state.phase){
    case UpdatePhase::Checking:return tr(L"Suche nach einer neuen Version …");
    case UpdatePhase::Current:return tr(L"Du verwendest die aktuelle Version.");
    case UpdatePhase::Available:return tr(L"Neue Version verfügbar. Installiere sie über die Release-Seite.");
    case UpdatePhase::Downloading:return tr(L"Update wird im Hintergrund heruntergeladen …");
    case UpdatePhase::Ready:description=tr(L"Version ")+std::wstring(state.version.begin(),state.version.end())+(app.preferences.autoUpdates?tr(L" wird beim Beenden installiert."):tr(L" ist bereit zur Installation."));return description.c_str();
    case UpdatePhase::Failed:return tr(L"Updates konnten nicht geprüft werden. Versuche es später erneut.");
    default:return tr(L"Version " FOTO_VERSION_WSTRING L" · Lyvt-Dev · MIT");}
}
foto::ViewerLayout viewLayout();
foto::Box exportNoticeBox(){auto l=viewLayout();float width=std::min(460.0f,l.canvas.w-48);return {l.canvas.w-width-24,120,width,124};}

namespace ui {
Color background(255,39,39,39),surface(255,32,32,32),raised(255,49,49,49),line(255,63,63,63);
Color text(255,245,245,245),muted(255,198,198,198),faint(255,148,148,148),accent(255,76,194,255);
void palette(){bool light=app.preferences.theme==2;
    if(app.preferences.theme==0){DWORD value=0,bytes=sizeof(value);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&bytes)==ERROR_SUCCESS)light=value!=0;}
    background=light?Color(255,249,249,249):Color(255,24,28,36);surface=light?Color(255,243,243,243):Color(255,18,22,29);raised=light?Color(255,255,255,255):Color(255,34,40,50);
    line=light?Color(255,219,219,219):Color(255,63,63,63);text=light?Color(255,26,26,26):Color(255,245,245,245);muted=light?Color(255,85,85,85):Color(255,198,198,198);faint=light?Color(255,117,117,117):Color(255,148,148,148);accent=Color(255,GetRValue(app.preferences.accent),GetGValue(app.preferences.accent),GetBValue(app.preferences.accent));
    BOOL dark=!light;DwmSetWindowAttribute(app.window,20,&dark,sizeof(dark));COLORREF caption=light?RGB(243,243,243):RGB(32,32,32);DwmSetWindowAttribute(app.window,35,&caption,sizeof(caption));
}
void fill(Graphics& g,Color color,foto::Box r){SolidBrush brush(color);g.FillRectangle(&brush,r.x,r.y,r.w,r.h);}
void round(Graphics& g,Color color,foto::Box r,float radius=-1) {
    if(r.w<=0||r.h<=0)return;
    if(radius<0)radius=float(app.preferences.radius);
    if(radius<=0){fill(g,color,r);return;}
    float d=std::min(radius*2,std::min(r.w,r.h));GraphicsPath path;
    path.AddArc(r.x,r.y,d,d,180,90);path.AddArc(r.x+r.w-d,r.y,d,d,270,90);
    path.AddArc(r.x+r.w-d,r.y+r.h-d,d,d,0,90);path.AddArc(r.x,r.y+r.h-d,d,d,90,90);path.CloseFigure();
    SolidBrush brush(color);g.FillPath(&brush,&path);
}
std::map<std::pair<float,bool>,std::unique_ptr<Font>> fonts;
void outline(Graphics& g,Color color,foto::Box r,float radius=-1,float width=1){
    if(radius<0)radius=float(app.preferences.radius);GraphicsPath path;float d=std::min(radius*2,std::min(r.w,r.h));
    if(d<=0)path.AddRectangle(RectF(r.x,r.y,r.w,r.h));else{path.AddArc(r.x,r.y,d,d,180,90);path.AddArc(r.x+r.w-d,r.y,d,d,270,90);path.AddArc(r.x+r.w-d,r.y+r.h-d,d,d,0,90);path.AddArc(r.x,r.y+r.h-d,d,d,90,90);path.CloseFigure();}
    Pen pen(color,width);g.DrawPath(&pen,&path);
}
void label(Graphics& g,const std::wstring& textValue,foto::Box r,float size=13,Color color=text,bool bold=false,bool center=false,bool wrap=false) {
    auto& font=fonts[{size,bold}];if(!font){FontFamily family(L"Segoe UI");font=std::make_unique<Font>(family.GetLastStatus()==Ok?&family:FontFamily::GenericSansSerif(),size,bold?FontStyleBold:FontStyleRegular,UnitPixel);}
    SolidBrush brush(color);StringFormat format;format.SetTrimming(StringTrimmingEllipsisCharacter);
    format.SetLineAlignment(StringAlignmentCenter);if(!wrap)format.SetFormatFlags(StringFormatFlagsNoWrap);
    if(center)format.SetAlignment(StringAlignmentCenter);
    g.DrawString(textValue.c_str(),-1,font.get(),RectF(r.x,r.y,r.w,r.h),&format,&brush);
}
void icon(Graphics& g,Icon type,float x,float y,Color color=text,float size=20) {
    auto state=g.Save();g.TranslateTransform(x,y);g.ScaleTransform(size/24,size/24);
    Pen p(color,1.65f);p.SetStartCap(LineCapRound);p.SetEndCap(LineCapRound);p.SetLineJoin(LineJoinRound);
    auto l=[&](float a,float b,float c,float d){g.DrawLine(&p,a,b,c,d);};
    switch(type) {
    case Icon::Menu:l(4,6,20,6);l(4,12,20,12);l(4,18,20,18);break;
    case Icon::Heart:{PointF pts[]={{12,20},{3,11},{3,6},{7,3},{12,7},{17,3},{21,6},{21,11},{12,20}};g.DrawLines(&p,pts,9);break;}
    case Icon::Settings:g.DrawEllipse(&p,8.0f,8.0f,8.0f,8.0f);for(int i=0;i<8;++i){float a=float(i)*3.1415926f/4;l(12+8*std::cos(a),12+8*std::sin(a),12+11*std::cos(a),12+11*std::sin(a));}g.DrawEllipse(&p,4.0f,4.0f,16.0f,16.0f);break;
    case Icon::Search:g.DrawEllipse(&p,4.0f,3.0f,12.0f,12.0f);l(14,14,21,21);break;
    case Icon::Mirror:l(12,3,12,21);l(3,6,9,12);l(9,12,3,18);l(3,18,3,6);l(21,6,15,12);l(15,12,21,18);l(21,18,21,6);break;
    case Icon::Convert:g.DrawArc(&p,4.0f,4.0f,16.0f,16.0f,200.0f,130.0f);g.DrawArc(&p,4.0f,4.0f,16.0f,16.0f,20.0f,130.0f);l(20,3,20,9);l(20,9,14,9);l(4,21,4,15);l(4,15,10,15);break;
    case Icon::Sort:l(7,3,7,21);l(3,7,7,3);l(7,3,11,7);l(17,3,17,21);l(13,17,17,21);l(17,21,21,17);break;
    case Icon::Grid:for(int y=4;y<20;y+=10)for(int x=4;x<20;x+=10)g.DrawRectangle(&p,float(x),float(y),6.0f,6.0f);break;
    case Icon::Check:l(5,12,10,17);l(10,17,20,6);break;
    case Icon::Window:g.DrawRectangle(&p,6.0f,6.0f,12.0f,12.0f);break;
    case Icon::Computer:g.DrawRectangle(&p,3.0f,3.0f,18.0f,13.0f);l(12,16,12,21);l(6,21,18,21);break;
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
Com<IStream> brandStream;
std::unique_ptr<Bitmap> brandBitmap;
void releaseBrand(){fonts.clear();brandBitmap.reset();brandStream.out();}
void brand(Graphics& g,float x,float y,float size=28) {
    if(!brandBitmap){auto module=GetModuleHandleW(nullptr);auto resource=FindResourceW(module,MAKEINTRESOURCEW(10),RT_RCDATA);
        if(resource){auto data=LoadResource(module,resource);brandStream.p=SHCreateMemStream(static_cast<const BYTE*>(LockResource(data)),SizeofResource(module,resource));
            if(brandStream.p)brandBitmap.reset(Bitmap::FromStream(brandStream));}}
    if(brandBitmap&&brandBitmap->GetLastStatus()==Ok)g.DrawImage(brandBitmap.get(),RectF(x,y,size,size));
}
void drawPicture(Graphics& g,const Picture& p,foto::Box box,bool crop=false) {
    float scale=crop?std::max(box.w/p.width,box.h/p.height):std::min(box.w/p.width,box.h/p.height);
    float w=p.width*scale,h=p.height*scale;auto saved=g.Save();g.SetClip(RectF(box.x,box.y,box.w,box.h),CombineModeIntersect);
    Bitmap bitmap(p.width,p.height,p.width*4,PixelFormat32bppPARGB,const_cast<BYTE*>(p.pixels.data()));
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.DrawImage(&bitmap,RectF(box.x+(box.w-w)/2,box.y+(box.h-h)/2,w,h));g.Restore(saved);
}
}
foto::ViewerLayout viewLayout() {
    RECT r{};GetClientRect(app.window,&r);float scale=app.dpi/96.0f;
    return {r.right/scale,r.bottom/scale,!app.gallery&&!app.settings&&!app.files.empty(),app.info,app.strip};
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
    if(key=='S'&&GetKeyState(VK_CONTROL)<0&&!app.gallery&&!app.settings)command(Export);
    else if(key=='C'&&GetKeyState(VK_CONTROL)<0&&!app.gallery&&!app.settings)command(CopyRaster);
    else if(key=='O'&&GetKeyState(VK_CONTROL)<0)command(GetKeyState(VK_SHIFT)<0?OpenFolder:Open);
    else if(key==VK_F1)command(HelpKeys);
    else if(key==VK_ESCAPE) {
        if(app.helpPage>=0)command(CloseHelp);else if(app.settings)command(Gallery);else if(app.fullscreen)command(Full);else if(app.slideshow)command(Slide);else if(!app.gallery)command(Gallery);
    }else if(app.helpPage<0&&!app.gallery&&!app.settings) {
        switch(key) {
        case VK_LEFT:command(Prev);break;case VK_RIGHT:command(Next);break;
        case 'F':command(Fit);break;case '1':command(Actual);break;case 'R':command(Rotate);break;case 'M':command(Mirror);break;
        case 'I':command(Info);break;case 'T':command(Strip);break;case VK_F11:command(Full);break;
        case VK_SPACE:command(Slide);break;case VK_ADD:case VK_OEM_PLUS:command(ZoomIn);break;
        case VK_SUBTRACT:case VK_OEM_MINUS:command(ZoomOut);break;
        default:return false;
        }
    }else return false;
    return true;
}
bool motionEnabled(){BOOL enabled=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);return app.preferences.animations&&enabled;}
void animateButtons(){if(motionEnabled())SetTimer(app.window,4,16,nullptr);else for(auto& entry:app.controls){entry.second.hoverProgress=app.hover==entry.first?1.0f:0.0f;InvalidateRect(entry.second.window,nullptr,FALSE);}}
Color blend(Color from,Color to,float progress){return Color(255,BYTE(from.GetR()+(to.GetR()-from.GetR())*progress),BYTE(from.GetG()+(to.GetG()-from.GetG())*progress),BYTE(from.GetB()+(to.GetB()-from.GetB())*progress));}
LRESULT CALLBACK buttonProc(HWND window,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR id) {
    switch(msg) {
    case WM_ERASEBKGND:return 1;
    case WM_MOUSEWHEEL:return SendMessageW(app.window,msg,wp,lp);
    case WM_PAINT:case WM_PRINTCLIENT:{
        PAINTSTRUCT paint{};HDC dc=msg==WM_PAINT?BeginPaint(window,&paint):reinterpret_cast<HDC>(wp);
        RECT bounds{};GetClientRect(window,&bounds);DRAWITEMSTRUCT item{};item.CtlType=ODT_BUTTON;item.CtlID=UINT(id);item.hwndItem=window;item.hDC=dc;item.rcItem=bounds;item.itemAction=ODA_DRAWENTIRE;
        auto state=SendMessageW(window,BM_GETSTATE,0,0);if(state&BST_PUSHED)item.itemState|=ODS_SELECTED;if(GetFocus()==window)item.itemState|=ODS_FOCUS;if(!IsWindowEnabled(window))item.itemState|=ODS_DISABLED;
        drawButton(item);if(msg==WM_PAINT)EndPaint(window,&paint);return 0;
    }
    case WM_MOUSEMOVE: {
        if(app.hover!=int(id)) {int old=app.hover;app.hover=int(id);animateButtons();if(app.controls.count(old))InvalidateRect(app.controls[old].window,nullptr,FALSE);InvalidateRect(window,nullptr,FALSE);}
        TRACKMOUSEEVENT tracking{sizeof(tracking),TME_LEAVE,window,0};TrackMouseEvent(&tracking);break;
    }
    case WM_MOUSELEAVE:if(app.hover==int(id)){app.hover=0;animateButtons();InvalidateRect(window,nullptr,FALSE);}break;
    case WM_SETFOCUS:case WM_KILLFOCUS:InvalidateRect(window,nullptr,FALSE);break;
    case WM_KEYDOWN:
        if(wp==VK_RETURN){command(int(id));return 0;}
        if(wp!=VK_SPACE&&shortcut(UINT(wp)))return 0;
        break;
    case WM_NCDESTROY:if(app.controls.count(int(id)))app.controls[int(id)].buffer.reset();RemoveWindowSubclass(window,buttonProc,id);break;
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
    if(c.caption!=caption){c.caption=caption;c.accessible=caption;SetWindowTextW(c.window,caption.c_str());}
    auto wantedTip=tip.empty()?caption:tip;
    if(c.tip!=wantedTip) {
        c.tip=wantedTip;TOOLINFOW tool{};tool.cbSize=sizeof(tool);tool.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tool.hwnd=app.window;tool.uId=reinterpret_cast<UINT_PTR>(c.window);tool.lpszText=c.tip.data();
        SendMessageW(app.tooltip,TTM_UPDATETIPTEXTW,0,reinterpret_cast<LPARAM>(&tool));
    }
    float scale=app.dpi/96.0f;
    RECT pixels{int(std::round(box.x*scale)),int(std::round(box.y*scale)),int(std::round(box.w*scale)),int(std::round(box.h*scale))};
    if(!EqualRect(&pixels,&c.pixelBounds)){SetWindowPos(c.window,nullptr,pixels.left,pixels.top,pixels.right,pixels.bottom,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);c.pixelBounds=pixels;}
    if(bool(IsWindowEnabled(c.window))!=enabled)EnableWindow(c.window,enabled);
    if(!(GetWindowLongW(c.window,GWL_STYLE)&WS_VISIBLE))ShowWindow(c.window,SW_SHOWNA);
}
void clipControl(int id,foto::Box viewport){
    auto& c=app.controls[id];c.wantsClip=true;float scale=app.dpi/96.0f;
    RECT clip{std::max(0L,int(std::ceil(viewport.x*scale))-c.pixelBounds.left),std::max(0L,int(std::ceil(viewport.y*scale))-c.pixelBounds.top),
        std::min(c.pixelBounds.right,int(std::floor((viewport.x+viewport.w)*scale))-c.pixelBounds.left),std::min(c.pixelBounds.bottom,int(std::floor((viewport.y+viewport.h)*scale))-c.pixelBounds.top)};
    if(!c.clipped||!EqualRect(&clip,&c.clipBounds)){HRGN region=CreateRectRgn(clip.left,clip.top,clip.right,clip.bottom);if(!SetWindowRgn(c.window,region,FALSE))DeleteObject(region);c.clipBounds=clip;c.clipped=true;}
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
float contentLeft();
bool shellNavigation();
void layoutGalleryControls();
void layoutSettingsControls();
void layoutShell();
void layoutControls() {
    if(!app.window)return;
    for(auto& entry:app.controls){entry.second.visible=false;entry.second.wantsClip=false;}
    auto l=viewLayout();float w=l.width;bool file=!app.gallery&&!app.settings&&!app.files.empty(),viewer=file&&app.helpPage<0,loaded=bool(app.picture)&&!app.busy;
    layoutShell();
    if(app.helpPage>=0) {
        place(CloseHelp,{24,78,112,34},tr(L"Zurück"),Icon::Left);
        float x=std::max(contentLeft(),(w-800)/2),y=198;
        place(About,{x,y,128,38},tr(L"Über die App"),Icon::None,true,app.helpPage==0);
        place(HelpKeys,{x+138,y,132,38},tr(L"Tastenkürzel"),Icon::None,true,app.helpPage==1);
        place(HelpFormats,{x+280,y,132,38},tr(L"Dateiformate"),Icon::None,true,app.helpPage==2);
        if(app.helpPage==0) {
            place(ReleasePage,{x,426,174,38},tr(L"Versionen & Updates"),Icon::External);
            place(SourcePage,{x+184,426,150,38},tr(L"Projekt auf GitHub"),Icon::External);
        }else if(app.helpPage==2)place(ReleasePage,{x,462,196,38},tr(L"Vollständige Formatliste"),Icon::External);
    }else if(app.settings) {layoutSettingsControls();
    }else if(app.gallery||!file) {layoutGalleryControls();
    }else {
        place(Export,{62,74,136,30},tr(L"Konvertieren"),Icon::Convert,loaded&&!app.exporting,false,true,tr(L"Als anderes Format speichern (Strg+S)"));
        if(!app.lastExport.empty())place(RevealSaved,{260,74,38,30},tr(L"Gespeicherte Kopie anzeigen"),Icon::Folder);
        place(Mirror,{210,74,38,30},tr(L"Spiegeln"),Icon::Mirror,loaded,app.mirrored,false,tr(L"Horizontal spiegeln (M)"));
        place(Home,{18,75,32,28},tr(L"Startseite"),Icon::Home,true,false,false,tr(L"Zur Startseite"));
        place(Strip,{w-250,74,126,30},tr(L"Filmstreifen"),Icon::Strip,true,app.strip,false,tr(L"Filmstreifen anzeigen oder ausblenden (T)"));
        place(Info,{w-114,74,94,30},L"Details",Icon::Info,true,app.info,false,tr(L"Bildinformationen anzeigen oder ausblenden (I)"));
        float toolbarWidth=452,tx=std::max(8.0f,(l.canvas.w-toolbarWidth)/2),ty=l.canvas.y+l.canvas.h-58;
        place(Fit,{tx+6,ty+5,112,34},tr(L"Anpassen"),Icon::Fit,loaded,app.fit,false,tr(L"Bild an das Fenster anpassen (F)"));
        place(Actual,{tx+122,ty+5,38,34},L"1:1",Icon::None,loaded,!app.fit&&app.zoom==1,false,tr(L"Originalgröße (1)"));
        place(ZoomOut,{tx+172,ty+5,32,34},tr(L"Verkleinern"),Icon::Minus,loaded,false,false,tr(L"Verkleinern (−)"));
        place(ZoomMenu,{tx+204,ty+5,56,34},std::to_wstring(int(std::round(app.zoom*100)))+L" %",Icon::None,loaded,false,false,tr(L"Zoomstufe auswählen"));
        place(ZoomIn,{tx+260,ty+5,32,34},tr(L"Vergrößern"),Icon::Plus,loaded,false,false,tr(L"Vergrößern (+)"));
        place(Rotate,{tx+304,ty+5,34,34},tr(L"Drehen"),Icon::Rotate,loaded,false,false,tr(L"90° nach rechts drehen (R)"));
        place(Favorite,{l.width-306,74,42,30},tr(L"Favorit"),Icon::Heart,loaded,std::find(app.preferences.favorites.begin(),app.preferences.favorites.end(),app.files[app.index])!=app.preferences.favorites.end());
        place(Full,{tx+340,ty+5,34,34},tr(L"Vollbild"),Icon::Full,true,app.fullscreen,false,tr(L"Vollbild ein-/ausschalten (F11)"));
        place(Slide,{tx+394,ty+5,46,34},app.slideshow?tr(L"Diashow anhalten"):tr(L"Diashow starten"),app.slideshow?Icon::Pause:Icon::Play,app.files.size()>1,app.slideshow,false,tr(L"Diashow starten oder anhalten (Leertaste)"));
        if(app.exportNotice){auto panel=exportNoticeBox();
            place(OpenSaved,{panel.x+18,panel.y+82,126,30},tr(L"Kopie öffnen"),Icon::Image);
            place(RevealSaved,{panel.x+154,panel.y+82,162,30},tr(L"Im Ordner anzeigen"),Icon::Folder);
            place(CloseExport,{panel.x+panel.w-34,panel.y+8,26,26},tr(L"Schließen"),Icon::Close);
        }
        if(app.files.size()>1) {
            place(Prev,{8,l.image.y+l.image.h/2-22,32,44},tr(L"Vorheriges Bild"),Icon::Left,true,false,false,tr(L"Vorheriges Bild (←)"));
            place(Next,{l.canvas.w-40,l.image.y+l.image.h/2-22,32,44},tr(L"Nächstes Bild"),Icon::Right,true,false,false,tr(L"Nächstes Bild (→)"));
        }
        if(app.info) {
            place(Reveal,{l.inspector.x+20,l.inspector.y+l.inspector.h-58,156,34},tr(L"Im Explorer zeigen"),Icon::External);
            place(CopyPath,{l.inspector.x+184,l.inspector.y+l.inspector.h-58,34,34},tr(L"Dateipfad kopieren"),Icon::Copy);
        }
        if(!app.error.empty()) {
            float cx=l.image.x+l.image.w/2,cy=l.image.y+l.image.h/2;
            place(ErrorDetails,{cx-125,cy+72,122,34},tr(L"Fehlerdetails"),Icon::None);
            place(Formats,{cx+7,cy+72,122,34},tr(L"Formathilfe"),Icon::Help);
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
    for(auto& entry:app.controls){auto& c=entry.second;if(!c.visible&&c.window&&(GetWindowLongW(c.window,GWL_STYLE)&WS_VISIBLE))ShowWindow(c.window,SW_HIDE);if(c.visible&&c.clipped&&!c.wantsClip){SetWindowRgn(c.window,nullptr,FALSE);c.clipped=false;}}
    (void)viewer;
}
void refresh() {
    if(!app.window)return;
    ui::palette();updateZoom();layoutControls();
    RedrawWindow(app.window,nullptr,nullptr,RDW_INVALIDATE|RDW_NOERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
void drawButton(const DRAWITEMSTRUCT& item) {
    auto it=app.controls.find(int(item.CtlID));if(it==app.controls.end())return;
    auto& c=it->second;float scale=app.dpi/96.0f,w=c.box.w,h=c.box.h;
    HDC memory=c.buffer.prepare(item.hDC,item.rcItem.right,item.rcItem.bottom);if(!memory)memory=item.hDC;
    {Graphics g(memory);g.ScaleTransform(scale,scale);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    Color parent=ui::background;
    if(c.box.y<64 || c.id==NavToggle || c.id==Gallery || c.id==Favorites || c.id==Settings || c.id==ImportFolder||((c.id==OpenFolder||c.id==About)&&shellNavigation()))parent=ui::surface;
    if(c.id>=ThumbBase)parent=ui::surface;
    if(c.id==Reveal||c.id==CopyPath)parent=ui::surface;
    if(c.id>=Fit&&c.id<=Slide&&c.id!=Info&&c.id!=Formats)parent=ui::surface;
    if(c.id==ZoomMenu||c.id==OpenSaved||c.id==CloseExport||c.id==RevealSaved)parent=ui::surface;
    ui::fill(g,parent,{0,0,w,h});
    bool setting=app.settings&&(c.id==Theme||c.id==Language||c.id==DefaultZoom||c.id==TileInfo||c.id==Strip||c.id==RemoveFolder||c.id==ClearRecent||c.id==ReleasePage||(c.id>=Accent&&c.id<=CheckUpdates));
    if(setting)parent=ui::raised;
    g.Clear(parent);
    bool hot=app.hover==c.id,pressed=(item.itemState&ODS_SELECTED)!=0,focused=(item.itemState&ODS_FOCUS)!=0;
    Color fg=c.enabled?ui::text:ui::faint;
    Color fill=pressed?ui::line:blend(parent,ui::raised,c.hoverProgress);
    if(c.id==Open||c.id==SelectMode||c.id==ImportFolder)fill=blend(ui::raised,ui::line,c.hoverProgress);
    if(setting)fill=blend(ui::raised,ui::text,0.055f+c.hoverProgress*0.035f);
    if(c.active){fill=pressed?ui::line:blend(ui::raised,ui::line,c.hoverProgress);fg=ui::text;}
    if(c.id==Exit&&hot){fill=Color(255,196,43,28);fg=Color(255,255,255,255);}
    if(c.primary){fill=blend(ui::accent,ui::line,pressed?0.35f:c.hoverProgress*0.15f);fg=(int(fill.GetR())*299+int(fill.GetG())*587+int(fill.GetB())*114)>140000?Color(255,0,0,0):Color(255,255,255,255);}
    if(c.id>=GalleryBase) {
        ui::round(g,fill,{0,0,w,h});
        if(c.item<app.filtered.size()){auto thumb=app.thumbs.find(app.filtered[c.item]);
            if(thumb!=app.thumbs.end()&&thumb->second)ui::drawPicture(g,*thumb->second,{4,4,w-8,h-(app.preferences.tileInfo?44:8)});
            else ui::icon(g,Icon::Image,w/2-16,h/2-22,ui::faint,32);
            if(app.preferences.tileInfo)ui::label(g,c.caption,{12,h-36,w-24,28},13,ui::text);
            if(app.selected.count(app.filtered[c.item])){ui::round(g,ui::accent,{w-34,8,26,26},4);ui::icon(g,Icon::Check,w-31,11,Color(255,0,0,0),20);}
            else if(std::find(app.preferences.favorites.begin(),app.preferences.favorites.end(),app.filtered[c.item])!=app.preferences.favorites.end())ui::icon(g,Icon::Heart,w-30,10,ui::accent,18);
        }
    }else if(c.id>=ThumbBase) {
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
    }else if(setting&&(c.id==TileInfo||c.id==Strip||c.id==Animations||c.id==Recursive||c.id==AutoUpdates)){
        bool on=c.id==TileInfo?app.preferences.tileInfo:c.id==Strip?app.strip:c.id==Animations?app.preferences.animations:c.id==AutoUpdates?app.preferences.autoUpdates:app.preferences.recursive;
        ui::label(g,c.caption,{0,0,42,h},14,ui::text);
        ui::round(g,on?ui::accent:ui::line,{50,(h-24)/2,52,24},12);
        ui::round(g,on?Color(255,20,20,20):ui::muted,{on?80.0f:54.0f,(h-16)/2,16,16},8);
    }else {
        bool elevated=setting||c.primary||c.active||c.id==Open||c.id==SelectMode||c.id==ImportFolder||c.id==WelcomeOpen||c.id==WelcomeFolder;
        foto::Box face=elevated?foto::Box{2,pressed?2.0f:1.0f,w-4,h-4}:foto::Box{0,0,w,h};
        if(elevated){ui::round(g,Color(pressed?25:55,0,0,0),{1,3,w-2,h-3});if(!setting&&!c.primary)fill=blend(fill,ui::text,0.035f);}
        ui::round(g,fill,face);
        if(elevated){ui::outline(g,blend(fill,ui::text,0.10f),{face.x+0.5f,face.y+0.5f,face.w-1,face.h-1});auto rim=g.Save();g.SetClip(RectF(face.x,face.y,face.w,face.h/2),CombineModeIntersect);ui::outline(g,blend(fill,ui::text,0.18f),{face.x+0.5f,face.y+0.5f,face.w-1,face.h-1});g.Restore(rim);}
        if(pressed)g.TranslateTransform(0,1);
        if(c.id==Accent){ui::round(g,ui::accent,{12,(h-18)/2,18,18},9);ui::label(g,c.caption,{40,0,w-48,h},14,fg);}
        else {
        bool iconOnly=c.icon!=Icon::None&&w<=48;
        if(iconOnly)ui::icon(g,c.icon,(w-19)/2,(h-19)/2,fg,19);
        else if(c.icon!=Icon::None) {
            ui::icon(g,c.icon,12,(h-18)/2,fg,18);
            ui::label(g,c.caption,{38,0,w-44,h},14,fg,false);
        }else if(setting&&(c.id==Theme||c.id==Language||c.id==DefaultZoom)){
            ui::label(g,c.caption,{12,0,w-42,h},14,fg);Pen chevron(fg,1.2f);g.DrawLine(&chevron,w-24,h/2-2,w-20,h/2+2);g.DrawLine(&chevron,w-20,h/2+2,w-16,h/2-2);
        }else ui::label(g,c.caption,{4,0,w-8,h},14,fg,false,true);
        }
    }
    if((c.id==Gallery&&app.gallery&&!app.favoritesView&&!app.settings)||(c.id==Favorites&&app.favoritesView&&!app.settings))ui::round(g,ui::accent,{0,9,3,h-18},1.5f);
    if(focused)ui::outline(g,ui::accent,{2.5f,2.5f,w-5,h-5},-1,1.5f);
    }if(memory!=item.hDC)BitBlt(item.hDC,0,0,item.rcItem.right,item.rcItem.bottom,memory,0,0,SRCCOPY);
}
void drawWelcome(Graphics& g,const foto::ViewerLayout& l) {
    float x=std::max(40.0f,(l.width-850)/2),y=std::max(132.0f,(l.height-370)/2);
    ui::round(g,ui::surface,{x,y,52,52},13);ui::icon(g,Icon::Image,x+13,y+13,ui::accent,26);
    ui::label(g,tr(L"Willkommen bei PAF-Photos"),{x,y+67,380,72},30,ui::text,true,false,true);
    ui::label(g,tr(L"Öffne ein Bild oder einen Ordner.\nDu kannst Dateien auch hierher ziehen."),{x,y+140,360,40},13,ui::muted,false,false,true);
    ui::label(g,tr(L"Strg + O zum Öffnen · F1 für Hilfe"),{x,y+242,350,24},11,ui::faint);
    float rx=x+450,rw=std::max(250.0f,l.width-rx-40),ry=y+36;
    if(l.width<960){rx=x;rw=l.width-2*x;ry=y+264;}
    ui::label(g,tr(L"Zuletzt geöffnet"),{rx,ry,rw,24},13,ui::text,true);
    if(app.preferences.recent.empty()) {
        ui::round(g,ui::surface,{rx,ry+34,rw,112},9);
        ui::icon(g,Icon::Image,rx+20,ry+56,ui::faint,23);
        ui::label(g,tr(L"Deine letzten Bilder erscheinen hier."),{rx+56,ry+50,rw-76,36},12,ui::muted,false,false,true);
        ui::label(g,tr(L"Nur auf diesem Gerät gespeichert."),{rx+20,ry+103,rw-40,22},11,ui::faint);
    }
}
void drawHelp(Graphics& g,const foto::ViewerLayout& l) {
    float x=std::max(contentLeft(),(l.width-800)/2),w=std::min(800.0f,l.width-x-32);
    ui::label(g,tr(L"Info & Hilfe"),{x,130,w,48},28,ui::text,true);
    ui::fill(g,ui::line,{x,248,w,1});
    if(app.helpPage==0) {
        ui::brand(g,x,278,48);
        ui::label(g,L"PAF-Photos",{x+66,276,w-66,32},25,ui::text,true);
        ui::label(g,L"Version " FOTO_VERSION_WSTRING L"  ·  Windows 64-Bit",{x+66,312,w-66,25},13,ui::accent);
        ui::label(g,tr(L"Ein schneller Bildbetrachter für deine Fotos, Illustrationen und Bilddateien."),{x,355,w,28},14);
        ui::label(g,tr(L"Entwickelt von Lyvt-Dev · MIT-Lizenz"),{x,389,w,24},12,ui::muted);
        ui::label(g,tr(L"Originaldateien werden beim Betrachten nicht verändert."),{x,488,w,24},12,ui::muted);
    }else if(app.helpPage==1) {
        const std::pair<const wchar_t*,const wchar_t*> keys[]={
            {tr(L"Strg + O"),tr(L"Bild öffnen")},{tr(L"Strg + Umschalt + O"),tr(L"Ordner öffnen")},{L"←  /  →",tr(L"Zwischen Bildern blättern")},
            {tr(L"Mausrad / + / −"),tr(L"Zoom ändern")},{L"F  /  1",tr(L"Anpassen / Originalgröße")},{L"R",tr(L"Bild nach rechts drehen")},
            {tr(L"Strg + S"),tr(L"Konvertieren")},{tr(L"Strg + C"),tr(L"Bild kopieren")},{L"M",tr(L"Spiegeln")},
            {L"F11",tr(L"Vollbild ein-/ausschalten")},{tr(L"Leertaste"),tr(L"Diashow starten / anhalten")},{L"I  /  T",tr(L"Details / Filmstreifen")},
            {tr(L"Doppelklick"),tr(L"Zwischen Anpassen und 1:1 wechseln")},{tr(L"Ziehen"),tr(L"Vergrößertes Bild verschieben")},{L"Esc",tr(L"Hilfe, Vollbild oder Diashow verlassen")},
            {tr(L"Tab / Umschalt + Tab"),tr(L"Bedienelemente auswählen")},{L"Enter",tr(L"Ausgewählte Schaltfläche auslösen")}};
        float cw=(w-28)/2;
        for(size_t i=0;i<std::size(keys);++i) {
            float cx=x+(i/9)*(cw+28),cy=269+float(i%9)*28;
            ui::label(g,keys[i].first,{cx,cy,cw*0.48f,30},11,ui::accent);
            ui::label(g,keys[i].second,{cx+cw*0.48f,cy,cw*0.52f,30},11,ui::text,false,false,true);
        }
    }else {
        ui::label(g,tr(L"Viele Bilder direkt öffnen"),{x,274,w,28},18,ui::text,true);
        ui::label(g,tr(L"PNG, JPEG, BMP, GIF, TIFF und ICO sowie WebP, SVG-Formen, PSD-Komposite, TGA, PGM/PPM, PIC und QOI."),{x,310,w,42},13,ui::muted,false,false,true);
        ui::label(g,tr(L"Weitere Formate mit zusätzlichen Decodern"),{x,362,w,26},16,ui::text,true);
        ui::label(g,tr(L"Installierte Windows-Codecs und optional ImageMagick erweitern die Unterstützung. Proprietäre Projekte sind nicht durchgehend lesbar. Animationen und mehrseitige Dateien zeigen das erste Bild."),{x,393,w,56},12,ui::muted,false,false,true);
        ui::label(g,magickPath().empty()?tr(L"ImageMagick: nicht eingerichtet"):tr(L"ImageMagick: verfügbar"),{x+214,462,w-214,38},11,ui::muted);
    }
}
void drawInspector(Graphics& g,const foto::ViewerLayout& l) {
    auto r=l.inspector;if(r.w==0)return;
    ui::fill(g,ui::surface,r);ui::fill(g,ui::line,{r.x,r.y,1,r.h});
    float x=r.x+22,y=r.y+22,w=r.w-44;
    ui::label(g,tr(L"Bilddetails"),{x,y,w,24},14,ui::text,true);
    ui::label(g,app.files[app.index].filename().wstring(),{x,y+38,w,52},17,ui::text,true,false,true);
    ui::fill(g,ui::line,{x,y+110,w,1});
    auto row=[&](float yy,const wchar_t* name,const std::wstring& value) {
        ui::label(g,name,{x,yy,w,18},11,ui::muted);ui::label(g,value,{x,yy+21,w,23},13);
    };
    row(y+132,tr(L"Abmessungen"),app.picture?std::to_wstring(app.picture->originalW)+L" × "+std::to_wstring(app.picture->originalH)+L" px":L"—");
    row(y+196,tr(L"Dateigröße"),app.fileSize);
    auto extension=app.files[app.index].extension().wstring();if(!extension.empty())extension.erase(0,1);
    std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return wchar_t(towupper(c));});
    row(y+260,tr(L"Dateiformat"),extension.empty()?tr(L"Unbekannt"):extension);
    if(r.h>490)row(y+324,tr(L"Zuletzt geändert"),app.fileDate);
    if(app.picture&&app.picture->frames>1&&r.h>560)ui::label(g,tr(L"Vorschau: erstes Bild / erste Seite"),{x,y+394,w,40},11,ui::muted,false,false,true);
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
        g.TranslateTransform(cx,cy);g.RotateTransform(float(app.rotation*90));if(app.mirrored)g.ScaleTransform(-1.0f,1.0f);
        g.SetInterpolationMode(app.zoom>3?InterpolationModeNearestNeighbor:InterpolationModeHighQualityBicubic);
        Bitmap bitmap(p.width,p.height,p.width*4,PixelFormat32bppPARGB,p.pixels.data());
        g.DrawImage(&bitmap,RectF(float(-double(p.width)*app.zoom/2),float(-double(p.height)*app.zoom/2),float(p.width*app.zoom),float(p.height*app.zoom)));
        g.Restore(state);
    }else {
        auto r=l.image;float cx=r.x+r.w/2,cy=r.y+r.h/2;
        if(app.busy) {
            Pen spin(ui::accent,2.5f);spin.SetStartCap(LineCapRound);spin.SetEndCap(LineCapRound);
            g.DrawArc(&spin,cx-12,cy-38,24.0f,24.0f,float(GetTickCount64()%1200)*0.3f,270.0f);
            ui::label(g,tr(L"Bild wird geöffnet …"),{r.x,cy+4,r.w,28},14,ui::muted,false,true);
        }else if(!app.error.empty()) {
            ui::icon(g,Icon::Image,cx-18,cy-82,ui::faint,36);
            ui::label(g,tr(L"Dieses Bild lässt sich nicht öffnen"),{r.x,cy-30,r.w,32},18,ui::text,true,true);
            ui::label(g,tr(L"Die Datei ist beschädigt oder benötigt einen zusätzlichen Decoder."),{r.x+16,cy+8,r.w-32,48},12,ui::muted,false,true,true);
        }
    }
    float tx=std::max(8.0f,(l.canvas.w-452)/2),ty=l.canvas.y+l.canvas.h-58;
    ui::round(g,ui::surface,{tx,ty,452,44});
    for(float offset:{165.0f,297.0f,383.0f})ui::fill(g,ui::line,{tx+offset,ty+12,1,20});
    if(app.strip) {
        ui::fill(g,ui::surface,l.filmstrip);ui::fill(g,ui::line,{0,l.filmstrip.y,l.filmstrip.w,1});
        ui::label(g,std::to_wstring(app.index+1)+tr(L" von ")+std::to_wstring(app.files.size())+tr(L" Bildern"),{20,l.filmstrip.y+5,l.filmstrip.w-40,20},10,ui::muted,false,true);
    }
    if(app.info)drawInspector(g,l);
}
void drawGallery(Graphics&,const foto::ViewerLayout&);
void drawSettings(Graphics&,const foto::ViewerLayout&);
void drawShell(Graphics&,const foto::ViewerLayout&);
void paint(HWND window) {
    PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT bounds{};GetClientRect(window,&bounds);
    if(bounds.right<=0||bounds.bottom<=0){EndPaint(window,&ps);return;}
    HDC memory=app.buffer.prepare(dc,bounds.right,bounds.bottom);if(!memory)memory=dc;
    {
        Graphics g(memory);float scale=app.dpi/96.0f;g.ScaleTransform(scale,scale);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        auto l=viewLayout();float w=l.width,h=l.height;g.SetClip(RectF(ps.rcPaint.left/scale,ps.rcPaint.top/scale,(ps.rcPaint.right-ps.rcPaint.left)/scale,(ps.rcPaint.bottom-ps.rcPaint.top)/scale));bool contentOnly=ps.rcPaint.top>=170*scale&&ps.rcPaint.bottom<=(h-28)*scale;ui::fill(g,ui::background,{0,0,w,h});ui::fill(g,ui::surface,{0,0,w,64});
        if(!contentOnly)drawShell(g,l);
        if(app.helpPage>=0)drawHelp(g,l);else if(app.settings)drawSettings(g,l);else if(app.gallery||app.files.empty())drawGallery(g,l);else {drawViewer(g,l);if(app.exportNotice){auto panel=exportNoticeBox();ui::round(g,ui::surface,panel);ui::icon(g,Icon::Check,panel.x+18,panel.y+15,ui::accent,22);
            ui::label(g,tr(L"Kopie gespeichert"),{panel.x+50,panel.y+10,panel.w-94,30},16,ui::text,true);
            ui::label(g,app.lastExport.wstring(),{panel.x+18,panel.y+42,panel.w-36,34},12,ui::muted,false,false,true);}}
        if(!contentOnly){ui::fill(g,ui::line,{0,h-28,w,1});
        std::wstring status=tr(L"Bereit");
        if(!app.gallery&&!app.settings&&!app.files.empty()&&app.helpPage<0) {
            status=std::to_wstring(app.index+1)+L" / "+std::to_wstring(app.files.size());
            if(app.picture)status+=L"   ·   "+std::to_wstring(app.picture->originalW)+L" × "+std::to_wstring(app.picture->originalH)+L" px   ·   "+app.fileSize;
            if(app.slideshow)status+=tr(L"   ·   Diashow läuft");
        }
        ui::label(g,app.exporting?tr(L"Bild wird konvertiert …"):app.notice.empty()?status:app.notice,{20,h-27,w-295,26},10,app.notice.empty()?ui::muted:ui::accent);
        ui::label(g,std::wstring(L"PAF-Photos " FOTO_VERSION_WSTRING)+tr(L"   ·   F1 Hilfe"),{w-230,h-27,210,26},10,ui::faint,false,true);}
    }
    if(memory!=dc)BitBlt(dc,0,0,bounds.right,bounds.bottom,memory,0,0,SRCCOPY);EndPaint(window,&ps);
}
float sidebarWidth(){return app.navExpanded?236.0f:64.0f;}
float contentLeft(){return sidebarWidth()+48;}
bool shellNavigation(){return app.gallery||app.settings||app.helpPage>=0;}
void layoutShell(){
    auto l=viewLayout();float w=l.width;bool navigation=shellNavigation();
    place(Minimize,{w-138,0,46,64},tr(L"Minimieren"),Icon::Minus);
    place(Maximize,{w-92,0,46,64},IsZoomed(app.window)?tr(L"Wiederherstellen"):tr(L"Maximieren"),Icon::Window);
    place(Exit,{w-46,0,46,64},tr(L"Schließen"),Icon::Close);
    place(Open,{w-340,12,122,40},tr(L"Öffnen"),Icon::Image,true,false,false,tr(L"Bild öffnen (Strg+O)"));
    place(Settings,{w-202,12,40,40},tr(L"Einstellungen"),Icon::Settings,true,app.settings);
    if(navigation){
        float side=sidebarWidth();
        place(NavToggle,{12,76,40,40},tr(L"Navigation umschalten"),Icon::Menu);
        place(Gallery,{6,128,side-12,44},app.navExpanded?tr(L"Galerie"):tr(L"Galerie"),Icon::Image,true,app.gallery&&!app.favoritesView&&!app.settings);
        place(Favorites,{6,178,side-12,44},tr(L"Favoriten"),Icon::Heart,true,app.favoritesView&&!app.settings);
        place(OpenFolder,{6,250,side-12,44},tr(L"Ordner hinzufügen"),Icon::Folder);
        place(About,{6,l.height-82,side-12,40},tr(L"Über PAF-Photos"),Icon::Info);
    }else place(OpenFolder,{w-392,12,40,40},tr(L"Ordner öffnen"),Icon::Folder);
    if(!app.search){
        app.search=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,0,0,1,1,app.window,reinterpret_cast<HMENU>(INT_PTR(Search)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(app.search,EM_SETLIMITTEXT,0,512);
    }
    float scale=app.dpi/96.0f;int fontHeight=int(16*scale);
    static int lastHeight=0;
    if(!app.searchFont||lastHeight!=fontHeight){if(app.searchFont)DeleteObject(app.searchFont);app.searchFont=CreateFontW(-fontHeight,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");lastHeight=fontHeight;SendMessageW(app.search,WM_SETFONT,reinterpret_cast<WPARAM>(app.searchFont),TRUE);}
    SetWindowPos(app.search,nullptr,int(90*scale),int(22*scale),int((w-522)*scale),int(25*scale),SWP_NOZORDER|SWP_NOACTIVATE);
    SendMessageW(app.search,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(tr(L"Dateinamen und Formate suchen")));
    ShowWindow(app.search,navigation?SW_SHOWNA:SW_HIDE);
}
void layoutGalleryControls(){
    auto l=viewLayout();float x=contentLeft(),w=l.width-x-42,top=152;
    bool compact=w<700;float right=l.width-42,folderWidth=compact?42:132;
    place(ImportFolder,{right-folderWidth,104,folderWidth,38},tr(L"Ordner"),Icon::Folder);right-=folderWidth+12;
    place(More,{right-42,104,42,38},tr(L"Weitere Optionen"),Icon::More);right-=54;
    place(GridSize,{right-42,104,42,38},tr(L"Vorschaugröße"),Icon::Grid);right-=54;
    place(Sort,{right-42,104,42,38},tr(L"Sortierung ändern"),Icon::Sort,true,app.reverseSort);right-=54;
    place(SelectMode,{right-(compact?42:112),104,compact?42.0f:112.0f,38},tr(L"Auswählen"),Icon::Check,true,app.selectMode);
    if(app.selectMode){
        place(Favorite,{x,top+16,160,36},tr(L"Favoriten umschalten"),Icon::Heart,!app.selected.empty());
        place(ClearSelection,{x+172,top+16,160,36},tr(L"Auswahl aufheben"),Icon::None,!app.selected.empty());top+=66;
    }
    if(app.filtered.empty()){
        float cx=x+w/2,y=std::max(264.0f,l.height/2-26);
        if(app.library.empty()){
            place(WelcomeFolder,{cx-172,y+94,180,42},tr(L"Ordner hinzufügen"),Icon::Folder);
            place(WelcomeOpen,{cx+20,y+94,150,42},tr(L"Bild öffnen"),Icon::Image);
        }
        return;
    }
    float gridTop=top+44,usable=std::max(180.0f,w);int columns=std::max(1,int((usable+12)/(app.tileSize+12)));
    float tile=(usable-float(columns-1)*12)/columns,height=tile*0.76f+(app.preferences.tileInfo?40:0);
    int rows=int((app.filtered.size()+columns-1)/columns);
    double viewportHeight=std::max(1.0f,l.height-gridTop-36),stride=height+12;
    app.galleryMaxScroll=std::max(0.0,rows*stride-12-viewportHeight);app.galleryScroll.constrain(app.galleryMaxScroll);
    size_t firstRow=size_t(app.galleryScroll.position/stride),start=firstRow*columns;
    bool queued=false;int control=0;
    for(size_t i=start;i<app.filtered.size()&&control<90;++i){
        float y=gridTop+float(i/columns)*float(stride)-float(app.galleryScroll.position);
        if(y>=l.height-36)break;
        int id=GalleryBase+control++;place(id,{x+float(i%columns)*(tile+12),y,tile,height},app.filtered[i].filename().wstring(),Icon::Image,true,app.selected.count(app.filtered[i])!=0,false,app.filtered[i].wstring());app.controls[id].item=i;
        clipControl(id,{x,gridTop,w,float(viewportHeight)});
        if(!app.requestedThumbs.count(app.filtered[i])){app.requestedThumbs.insert(app.filtered[i]);std::lock_guard<std::mutex> lock(app.mutex);app.thumbQueue.push_back({app.filtered[i],app.folderSerial});queued=true;}
    }
    if(queued)app.cv.notify_one();
    if(app.thumbs.size()>128){std::set<fs::path> visible;for(const auto& c:app.controls)if(c.first>=GalleryBase&&c.second.visible)visible.insert(app.filtered[c.second.item]);
        for(auto i=app.thumbs.begin();i!=app.thumbs.end()&&app.thumbs.size()>96;){if(!visible.count(i->first)){app.requestedThumbs.erase(i->first);i=app.thumbs.erase(i);}else ++i;}}
}
struct ScrollTrack{float top=0,height=0,thumb=0,position=0;double maximum=0;};
ScrollTrack scrollTrack(){auto l=viewLayout();ScrollTrack t;t.top=app.settings?170.0f:196.0f+(app.selectMode?66.0f:0.0f);t.height=std::max(1.0f,l.height-36-t.top);
    t.maximum=app.settings?std::max(0.0,1810.0-(l.height-36)):app.galleryMaxScroll;
    t.thumb=std::min(t.height,std::max(28.0f,float(t.height*t.height/(t.height+t.maximum))));
    double position=app.settings?app.settingsScroll.position:app.galleryScroll.position;
    t.position=t.top+(t.maximum>0?float(position/t.maximum)*(t.height-t.thumb):0);return t;}
void redrawScroll(){layoutControls();auto l=viewLayout();float scale=app.dpi/96.0f;RECT dirty{LONG(contentLeft()*scale),LONG((app.settings?170:196+(app.selectMode?66:0))*scale),LONG(l.width*scale),LONG((l.height-36)*scale)};
    RedrawWindow(app.window,&dirty,nullptr,RDW_INVALIDATE|RDW_NOERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);}
bool scrollFrame(double seconds){
    if(app.helpPage>=0||(!app.settings&&!app.gallery))return false;
    auto& motion=app.settings?app.settingsScroll:app.galleryScroll;
    bool active=false;if(motionEnabled())active=motion.advance(seconds);else motion.reset(motion.target);
    motion.constrain(scrollTrack().maximum);redrawScroll();return active;
}
void scrollWheel(short delta){if(!delta)return;auto track=scrollTrack();UINT lines=3;SystemParametersInfoW(SPI_GETWHEELSCROLLLINES,0,&lines,0);
    double distance=lines==WHEEL_PAGESCROLL?track.height*0.9:double(lines)*30;
    auto& motion=app.settings?app.settingsScroll:app.galleryScroll;motion.add(-double(delta)/WHEEL_DELTA*distance,track.maximum);
    if(!motionEnabled()){motion.reset(motion.target);redrawScroll();return;}
    if(!app.scrollAnimating){app.motionTime=std::chrono::steady_clock::now();app.scrollAnimating=true;SetTimer(app.window,6,16,nullptr);}
}
void scrollDrag(float y){auto track=scrollTrack();double fraction=track.height>track.thumb?std::clamp(double(y-track.top-app.scrollGrab)/(track.height-track.thumb),0.0,1.0):0;
    (app.settings?app.settingsScroll:app.galleryScroll).reset(fraction*track.maximum);redrawScroll();}
struct SettingRow{float y;const wchar_t* title;const wchar_t* description;Icon icon;int action;};
std::vector<SettingRow> settingsRows(){return {
    {202,tr(L"App-Design"),tr(L"Wähle das Erscheinungsbild von PAF-Photos."),Icon::Settings,Theme},
    {296,tr(L"Sprache"),tr(L"Die Oberfläche wird sofort aktualisiert."),Icon::Image,Language},
    {390,tr(L"Dateinamen in der Galerie"),tr(L"Zeige den Namen unter jedem Vorschaubild."),Icon::Grid,TileInfo},
    {484,tr(L"Akzentfarbe"),tr(L"Wähle eine eigene Farbe für aktive Elemente."),Icon::Settings,Accent},
    {578,tr(L"Eckenradius"),tr(L"Von kantig bis weich gerundet. Die Vorschau ist sofort sichtbar."),Icon::Grid,Radius},
    {672,tr(L"Animationen"),tr(L"Sanfte Hover- und Scroll-Bewegungen. Windows-Bewegungseinstellungen werden beachtet."),Icon::Play,Animations},
    {766,tr(L"JPEG-Qualität"),tr(L"Qualität beim Konvertieren. Transparenz wird bei JPEG weiß."),Icon::Image,Quality},
    {928,tr(L"Zoom-Voreinstellung"),tr(L"Wähle, wie Bilder beim Öffnen angezeigt werden."),Icon::Image,DefaultZoom},
    {1022,tr(L"Filmstreifen"),tr(L"Zeige weitere Bilder beim Betrachten eines Fotos."),Icon::Strip,Strip},
    {1270,tr(L"Unterordner einbeziehen"),tr(L"Suche auch in Unterordnern. Maximal 10.000 Bilder, keine Verzeichnislinks."),Icon::Folder,Recursive},
    {1364,tr(L"Galerieordner"),tr(L"Entferne einen Ordner aus der Galerie. Deine Dateien bleiben erhalten."),Icon::Folder,RemoveFolder},
    {1458,tr(L"Lokalen Verlauf löschen"),tr(L"Entferne die Liste der zuletzt geöffneten Bilder."),Icon::Info,ClearRecent},
    {1614,tr(L"PAF-Photos"),updateStatus(),Icon::Info,CheckUpdates},
    {1708,tr(L"Automatische Updates"),tr(L"Prüfe beim Start und täglich. Installierte Apps aktualisieren sich beim Beenden."),Icon::External,AutoUpdates}
};}
const wchar_t* settingValue(int id){
    static std::wstring value;
    switch(id){
    case AutoUpdates:return app.preferences.autoUpdates?tr(L"Ein"):tr(L"Aus");
    case CheckUpdates:{auto state=app.updates.snapshot();return state.phase==UpdatePhase::Checking?tr(L"Prüfen …"):state.phase==UpdatePhase::Downloading?tr(L"Herunterladen …"):state.phase==UpdatePhase::Ready?tr(L"Jetzt installieren"):state.phase==UpdatePhase::Available&&!app.installed?tr(L"Update herunterladen"):tr(L"Nach Updates suchen");}
    case Accent:return tr(L"Farbe wählen");
    case Radius:value=std::to_wstring(app.preferences.radius)+L" px";return value.c_str();
    case Quality:value=std::to_wstring(app.preferences.quality)+L" %";return value.c_str();
    case Animations:return app.preferences.animations?tr(L"Ein"):tr(L"Aus");
    case Recursive:return app.preferences.recursive?tr(L"Ein"):tr(L"Aus");
    case Theme:return app.preferences.theme==0?tr(L"System"):app.preferences.theme==1?tr(L"Dunkel"):tr(L"Hell");
    case Language:return app.preferences.language==0?tr(L"Systemsprache"):app.preferences.language==1?L"Deutsch":L"English";
    case TileInfo:return app.preferences.tileInfo?tr(L"Ein"):tr(L"Aus");
    case Strip:return app.strip?tr(L"Ein"):tr(L"Aus");
    case DefaultZoom:return app.preferences.actual?tr(L"Originalgröße"):tr(L"An Fenster anpassen");
    case RemoveFolder:return tr(L"Ordner entfernen");case ClearRecent:return tr(L"Löschen");default:return tr(L"Nach Updates suchen");
    }
}
void layoutSettingsControls(){
    auto l=viewLayout();float x=contentLeft(),w=std::min(850.0f,l.width-x-42);
    int maxScroll=std::max(0,int(1810-(l.height-36)));app.settingsScroll.constrain(maxScroll);
    for(auto row:settingsRows()){
        float y=row.y-float(app.settingsScroll.position);if(y+64<=170||y+24>=l.height-36)continue;
        float controlWidth=row.action==DefaultZoom?190:row.action==TileInfo||row.action==Strip||row.action==Animations||row.action==Recursive||row.action==AutoUpdates?112:170;
        place(row.action,{x+w-controlWidth-20,y+24,controlWidth,40},settingValue(row.action),Icon::None,row.action!=RemoveFolder||!app.preferences.folders.empty());
        clipControl(row.action,{x,170,w,l.height-206});
        auto& c=app.controls[row.action];auto accessible=std::wstring(row.title)+L": "+settingValue(row.action);if(c.accessible!=accessible){c.accessible=accessible;SetWindowTextW(c.window,accessible.c_str());}
    }
}
void drawShell(Graphics& g,const foto::ViewerLayout& l){
    ui::fill(g,ui::surface,{0,0,l.width,64});ui::brand(g,20,20,24);
    if(shellNavigation()){
        ui::round(g,ui::raised,{62,12,l.width-482,40},5);ui::icon(g,Icon::Search,68,23,ui::muted,18);
        float side=sidebarWidth();ui::fill(g,ui::surface,{0,64,side,l.height-92});ui::fill(g,ui::line,{side,64,1,l.height-92});ui::fill(g,ui::line,{12,235,side-24,1});
    }else{
        ui::label(g,L"PAF-Photos",{56,14,104,34},16,ui::text,true);
        if(!app.files.empty())ui::label(g,app.files[app.index].filename().wstring(),{180,14,l.width-595,34},14,ui::muted,false,true);
    }
}
void drawGallery(Graphics& g,const foto::ViewerLayout& l){
    float x=contentLeft(),w=l.width-x-42;ui::icon(g,app.favoritesView?Icon::Heart:Icon::Image,x,110,ui::text,26);
    ui::label(g,app.favoritesView?tr(L"Favoriten"):tr(L"Galerie"),{x+40,96,std::max(90.0f,app.controls[SelectMode].box.x-x-48),48},30,ui::text,true);
    auto count=std::to_wstring(app.filtered.size())+L" "+tr(L"Elemente");
    if(app.selectMode)count+=L" · "+std::to_wstring(app.selected.size())+L" "+tr(L"ausgewählt");
    ui::label(g,count,{x,151,w,26},16,ui::muted);
    if(app.galleryMaxScroll>0&&!app.filtered.empty()){auto t=scrollTrack();ui::round(g,ui::faint,{l.width-10,t.position,3,t.thumb},1.5f);}
    if(app.filtered.empty()){
        float y=std::max(320.0f,l.height/2);ui::brand(g,x+w/2-36,y-96,72);
        ui::label(g,app.library.empty()?tr(L"Los geht’s mit PAF-Photos"):app.favoritesView?tr(L"Noch keine Favoriten"):tr(L"Keine passenden Bilder"),{x,y,w,46},30,ui::text,false,true);
        ui::label(g,app.library.empty()?tr(L"Füge einen Bilderordner hinzu. Deine Fotos erscheinen dann hier."):app.favoritesView?tr(L"Markiere Fotos mit dem Herz, um sie hier wiederzufinden."):tr(L"Versuche einen anderen Dateinamen oder ein anderes Format."),{x,y+52,w,38},18,ui::muted,false,true,true);
    }
}
void drawSettings(Graphics& g,const foto::ViewerLayout& l){
    float x=contentLeft(),w=std::min(850.0f,l.width-x-42);ui::icon(g,Icon::Settings,x,112,ui::text,28);
    ui::label(g,tr(L"Einstellungen"),{x+42,96,w-42,48},30,ui::text,true);
    auto saved=g.Save();g.SetClip(RectF(x,170,w,l.height-206),CombineModeIntersect);
    const std::pair<float,const wchar_t*> sections[]={{164,tr(L"Personalisierung")},{890,tr(L"Bildbetrachtung")},{1232,tr(L"Bibliothek & Datenschutz")},{1576,tr(L"Über die App")}};
    for(const auto& section:sections){float y=section.first-float(app.settingsScroll.position);if(y+30>170&&y<l.height-36)ui::label(g,section.second,{x,y,w,30},18,ui::text,true);}
    for(auto row:settingsRows()){
        float y=row.y-float(app.settingsScroll.position);if(y+88<=170||y>=l.height-36)continue;ui::round(g,ui::raised,{x,y,w,88});ui::icon(g,row.icon,x+24,y+30,ui::text,26);
        ui::label(g,row.title,{x+74,y+14,w-290,30},17,ui::text);
        ui::label(g,row.description,{x+74,y+43,w-290,34},13,ui::muted,false,false,true);
    }
    g.Restore(saved);
    if(1810>l.height-36){auto t=scrollTrack();ui::round(g,ui::line,{l.width-10,t.position,3,t.thumb},1.5f);}
}

void choose(bool folder,bool import=false) {
    Com<IFileOpenDialog> dialog;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.out()))))return;
    DWORD flags=0;dialog->GetOptions(&flags);
    dialog->SetOptions(flags|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR|FOS_PATHMUSTEXIST|(folder?FOS_PICKFOLDERS:FOS_FILEMUSTEXIST));
    dialog->SetTitle(folder?tr(L"Bilderordner öffnen"):tr(L"Bild öffnen"));
    std::wstring patterns;
    if(!folder) {
        for(const auto& ext:foto::extensions()){if(!patterns.empty())patterns+=L';';patterns+=L"*."+std::wstring(ext.begin(),ext.end());}
        COMDLG_FILTERSPEC filters[]={{tr(L"Bilddateien"),patterns.c_str()},{tr(L"Alle Dateien"),L"*.*"}};dialog->SetFileTypes(2,filters);
    }
    fs::path last;
    if(!app.files.empty())last=app.files[app.index].parent_path();else if(!app.preferences.recent.empty())last=app.preferences.recent.front().parent_path();
    if(!last.empty()) {Com<IShellItem> item;if(SUCCEEDED(SHCreateItemFromParsingName(last.c_str(),nullptr,IID_PPV_ARGS(item.out()))))dialog->SetFolder(item);}
    if(SUCCEEDED(dialog->Show(app.window))) {
        Com<IShellItem> item;
        if(SUCCEEDED(dialog->GetResult(item.out()))) {PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){if(import){fs::path folderPath(path);if(std::find(app.preferences.folders.begin(),app.preferences.folders.end(),folderPath)==app.preferences.folders.end())app.preferences.folders.push_back(folderPath);app.rebuildLibrary();app.saveSettings();app.gallery=true;app.settings=false;refresh();}else app.openPath(path);CoTaskMemFree(path);}}
    }
}
void chooseExport(ExportFormat format) {
    if(!app.picture||app.busy||app.exporting||app.files.empty())return;
    auto& picture=*app.picture;
    if((picture.originalW>picture.width||picture.originalH>picture.height||picture.frames>1||picture.engine.find(L"ImageMagick")!=std::wstring::npos)&&
       MessageBoxW(app.window,tr(L"Exportiert wird das angezeigte Rasterbild mit seinen aktuellen Pixelmaßen. Bei Animationen oder mehrseitigen Dateien nur das erste Bild. Fortfahren?"),tr(L"Bild konvertieren"),MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;
    Com<IFileSaveDialog> dialog;
    if(FAILED(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.out()))))return;
    const wchar_t* names[]={L"PNG (*.png)",L"JPEG (*.jpg)",L"BMP (*.bmp)",L"TIFF (*.tiff)",L"WebP (*.webp)"};
    auto extension=exportExtension(format);std::wstring pattern=L"*"+std::wstring(extension);
    COMDLG_FILTERSPEC filter{names[int(format)],pattern.c_str()};
    dialog->SetFileTypes(1,&filter);dialog->SetFileTypeIndex(1);dialog->SetDefaultExtension(extension+1);
    Com<IShellItem> folder;if(SUCCEEDED(SHCreateItemFromParsingName(app.files[app.index].parent_path().c_str(),nullptr,IID_PPV_ARGS(folder.out()))))dialog->SetFolder(folder);
    dialog->SetOptions(FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR|FOS_PATHMUSTEXIST|FOS_OVERWRITEPROMPT|FOS_STRICTFILETYPES);
    dialog->SetTitle(tr(L"Bild konvertieren"));
    auto name=app.files[app.index].stem().wstring()+L"-PAFPhotos"+extension;dialog->SetFileName(name.c_str());
    if(FAILED(dialog->Show(app.window)))return;
    Com<IShellItem> item;if(FAILED(dialog->GetResult(item.out())))return;
    PWSTR chosen=nullptr;if(FAILED(item->GetDisplayName(SIGDN_FILESYSPATH,&chosen)))return;
    fs::path target(chosen);CoTaskMemFree(chosen);auto before=target;
    target.replace_extension(exportExtension(format));
    // Some shell implementations append the chosen extension twice.
    while(CompareStringOrdinal(target.stem().extension().c_str(),-1,target.extension().c_str(),-1,TRUE)==CSTR_EQUAL)target=target.parent_path()/target.stem();
    std::error_code error;
    if(target!=before&&fs::exists(target,error)&&MessageBoxW(app.window,tr(L"Die Zieldatei existiert bereits. Ersetzen?"),tr(L"Bild konvertieren"),MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    if(fs::equivalent(app.files[app.index],target,error)||CompareStringOrdinal(fs::absolute(app.files[app.index]).lexically_normal().c_str(),-1,fs::absolute(target).lexically_normal().c_str(),-1,TRUE)==CSTR_EQUAL){
        MessageBoxW(app.window,tr(L"Wähle einen neuen Dateinamen. Die Originaldatei bleibt unverändert."),tr(L"Bild konvertieren"),MB_OK|MB_ICONINFORMATION);return;}
    if(format==ExportFormat::WebP&&(picture.width>16383||picture.height>16383)){MessageBoxW(app.window,tr(L"WebP erlaubt maximal 16.383 Pixel pro Seite. Wähle für dieses Bild PNG oder TIFF."),tr(L"Bild konvertieren"),MB_OK|MB_ICONINFORMATION);return;}
    app.exportCopy(target,format);
    refresh();
}
void copyPath() {
    if(app.files.empty()||!OpenClipboard(app.window))return;
    auto path=app.files[app.index].wstring();HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,(path.size()+1)*sizeof(wchar_t));
    if(data) {
        auto target=GlobalLock(data);
        if(target) {
            memcpy(target,path.c_str(),(path.size()+1)*sizeof(wchar_t));GlobalUnlock(data);
            EmptyClipboard();
            if(SetClipboardData(CF_UNICODETEXT,data)){app.notice=tr(L"Dateipfad kopiert");SetTimer(app.window,3,2500,nullptr);}
            else GlobalFree(data);
        }else GlobalFree(data);
    }
    CloseClipboard();
}
void popup(int action) {
    HMENU menu=CreatePopupMenu();
    if(action==ZoomMenu) {
        for(int z:{25,50,100,200,400})AppendMenuW(menu,MF_STRING,400+z,(std::to_wstring(z)+L" %").c_str());
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,Fit,tr(L"An Fenster anpassen\tF"));
    }else {
        AppendMenuW(menu,MF_STRING,Open,tr(L"Bild öffnen …\tStrg+O"));AppendMenuW(menu,MF_STRING,OpenFolder,tr(L"Ordner öffnen …\tStrg+Umschalt+O"));
        if(!app.files.empty()) {
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,Export,tr(L"Konvertieren"));AppendMenuW(menu,MF_STRING,CopyRaster,tr(L"Bild kopieren"));AppendMenuW(menu,MF_STRING,ResetView,tr(L"Ansicht zurücksetzen"));AppendMenuW(menu,MF_STRING,Reveal,tr(L"Im Explorer zeigen"));AppendMenuW(menu,MF_STRING,CopyPath,tr(L"Dateipfad kopieren"));
            AppendMenuW(menu,MF_STRING,Home,tr(L"Zur Startseite"));
        }
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,HelpKeys,tr(L"Tastenkürzel\tF1"));AppendMenuW(menu,MF_STRING,HelpFormats,tr(L"Unterstützte Dateiformate"));
        AppendMenuW(menu,MF_STRING,About,tr(L"Über PAF-Photos"));
        if(!app.preferences.recent.empty()){AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,ClearRecent,tr(L"Liste »Zuletzt geöffnet« leeren"));}
    }
    RECT r{};GetWindowRect(app.controls[action].window,&r);
    int selected=int(TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTALIGN,r.right,r.bottom,0,app.window,nullptr));DestroyMenu(menu);
    if(action==ZoomMenu&&selected>=425) {
        auto l=viewLayout();zoomAt((selected-400)/(app.zoom*100),l.image.x+l.image.w/2,l.image.y+l.image.h/2);
    }else if(selected)command(selected);
}
void command(int id) {
    if(id>=650&&id<=654){chooseExport(static_cast<ExportFormat>(id-650));return;}
    if(id>=GalleryBase&&id<GalleryBase+100){auto c=app.controls.find(id);if(c!=app.controls.end()&&c->second.item<app.filtered.size()){auto path=app.filtered[c->second.item];if(app.selectMode){if(!app.selected.erase(path))app.selected.insert(path);refresh();}else app.openPath(path);}return;}
    if(id>=600&&id<=602){app.preferences.language=id-600;app.applyLanguage();app.saveSettings();refresh();return;}
    if(id>=610&&id<=612){app.preferences.theme=id-610;app.saveSettings();refresh();return;}
    if(id>=620&&id<=621){app.preferences.actual=id==621;app.saveSettings();refresh();return;}
    if(id>=ThumbBase&&id<ThumbBase+20) {
        auto it=app.controls.find(id);if(it!=app.controls.end()&&it->second.item<app.files.size()){app.index=it->second.item;app.request();}return;
    }
    if(id>=RecentBase&&id<RecentBase+4){size_t i=size_t(id-RecentBase);if(i<app.preferences.recent.size())app.openPath(app.preferences.recent[i]);return;}
    bool loaded=bool(app.picture)&&!app.busy;
    switch(id) {
    case Minimize:ShowWindow(app.window,SW_MINIMIZE);return;
    case Maximize:ShowWindow(app.window,IsZoomed(app.window)?SW_RESTORE:SW_MAXIMIZE);return;
    case Exit:SendMessageW(app.window,WM_CLOSE,0,0);return;
    case Open:case WelcomeOpen:choose(false);break;
    case OpenFolder:case WelcomeFolder:choose(true,app.gallery||app.settings);break;
    case Prev:case Next:if(app.files.size()>1){app.index=foto::neighbor(app.index,id==Next?1:-1,app.files.size());app.request();}return;
    case Fit:if(loaded){app.fit=true;app.panX=app.panY=0;}break;
    case Actual:if(loaded){app.fit=false;app.zoom=1;app.panX=app.panY=0;}break;
    case ZoomIn:case ZoomOut:if(loaded){auto l=viewLayout();zoomAt(id==ZoomIn?1.25:0.8,l.image.x+l.image.w/2,l.image.y+l.image.h/2);}return;
    case Mirror:if(loaded)app.mirrored=!app.mirrored;break;
    case ResetView:app.rotation=0;app.mirrored=false;app.fit=true;app.panX=app.panY=0;break;
    case CopyRaster:if(loaded){app.notice=copyPicture(*app.picture,app.rotation,app.mirrored,app.window)?tr(L"Bild kopiert"):tr(L"Bild konnte nicht kopiert werden.");SetTimer(app.window,3,2500,nullptr);}break;
    case Export:if(loaded&&!app.exporting){HMENU menu=CreatePopupMenu();const wchar_t* formats[]={L"PNG",L"JPEG",L"BMP",L"TIFF",L"WebP"};
        for(int i=0;i<5;++i)AppendMenuW(menu,MF_STRING,650+i,formats[i]);RECT r{};GetWindowRect(app.controls[Export].window,&r);
        int selected=int(TrackPopupMenu(menu,TPM_RETURNCMD,r.left,r.bottom,0,app.window,nullptr));DestroyMenu(menu);if(selected)command(selected);}return;
    case OpenSaved:if(!app.lastExport.empty())app.openPath(app.lastExport);return;
    case CloseExport:app.exportNotice=false;break;
    case CheckUpdates:checkUpdates(true);return;
    case AutoUpdates:app.preferences.autoUpdates=!app.preferences.autoUpdates;app.saveSettings();if(app.preferences.autoUpdates)checkUpdates();else app.updates.stop();break;
    case RevealSaved:if(!app.lastExport.empty()){auto item=ILCreateFromPathW(app.lastExport.c_str());if(item){SHOpenFolderAndSelectItems(item,0,nullptr,0);ILFree(item);}}break;
    case Rotate:if(loaded){app.rotation=(app.rotation+1)%4;app.panX=app.panY=0;}break;
    case Info:app.info=!app.info;app.saveSettings();break;
    case Strip:app.strip=!app.strip;app.saveSettings();break;
    case Home:case Gallery:case Favorites:
        if(app.slideshow)command(Slide);
        app.gallery=true;app.settings=false;app.favoritesView=id==Favorites;app.helpPage=-1;app.rebuildLibrary();SetWindowTextW(app.window,AppTitle);SetFocus(app.window);break;
    case Settings:if(app.slideshow)command(Slide);app.settings=true;app.helpPage=-1;app.settingsScroll.reset();SetFocus(app.window);break;
    case NavToggle:app.navExpanded=!app.navExpanded;break;
    case ImportFolder:choose(true,true);break;
    case SelectMode:app.selectMode=!app.selectMode;if(!app.selectMode)app.selected.clear();break;
    case ClearSelection:app.selected.clear();break;
    case Accent:{static COLORREF custom[16]{};CHOOSECOLORW picker{};picker.lStructSize=sizeof(picker);picker.hwndOwner=app.window;picker.rgbResult=app.preferences.accent;picker.lpCustColors=custom;picker.Flags=CC_FULLOPEN|CC_RGBINIT;if(ChooseColorW(&picker)){app.preferences.accent=picker.rgbResult;app.saveSettings();}break;}
    case Radius:case Quality:{HMENU menu=CreatePopupMenu();const int radii[]={0,4,8,12,16};const int qualities[]={80,90,95,100};int count=id==Radius?5:4;
        for(int i=0;i<count;++i){int value=id==Radius?radii[i]:qualities[i];auto caption=std::to_wstring(value)+(id==Radius?L" px":L" %");AppendMenuW(menu,MF_STRING,1+i,caption.c_str());}
        RECT r{};GetWindowRect(app.controls[id].window,&r);int selected=int(TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTALIGN,r.right,r.bottom,0,app.window,nullptr));DestroyMenu(menu);
        if(selected>=1&&selected<=count){if(id==Radius)app.preferences.radius=radii[selected-1];else app.preferences.quality=qualities[selected-1];app.saveSettings();}break;}
    case Animations:app.preferences.animations=!app.preferences.animations;animateButtons();app.saveSettings();break;
    case Recursive:app.preferences.recursive=!app.preferences.recursive;app.rebuildLibrary();app.saveSettings();break;
    case TileInfo:app.preferences.tileInfo=!app.preferences.tileInfo;app.saveSettings();break;
    case Sort:app.reverseSort=!app.reverseSort;app.filterLibrary();break;
    case GridSize:app.tileSize=app.tileSize==210?150:app.tileSize==150?280:210;app.galleryScroll.reset();break;
    case Favorite:{std::vector<fs::path> paths;if(app.gallery)paths.assign(app.selected.begin(),app.selected.end());else if(!app.files.empty())paths.push_back(app.files[app.index]);
        for(const auto& path:paths){auto i=std::find(app.preferences.favorites.begin(),app.preferences.favorites.end(),path);if(i==app.preferences.favorites.end())app.preferences.favorites.push_back(path);else app.preferences.favorites.erase(i);}app.saveSettings();app.rebuildLibrary();break;}
    case Language:case Theme:case DefaultZoom:{HMENU menu=CreatePopupMenu();int base=id==Language?600:id==Theme?610:620;
        const wchar_t* optionsLanguage[]={tr(L"Systemsprache"),L"Deutsch",L"English"};const wchar_t* optionsTheme[]={tr(L"System"),tr(L"Dunkel"),tr(L"Hell")};const wchar_t* optionsZoom[]={tr(L"An Fenster anpassen"),tr(L"Originalgröße")};
        int current=id==Language?app.preferences.language:id==Theme?app.preferences.theme:app.preferences.actual?1:0;
        for(int i=0;i<(id==DefaultZoom?2:3);++i)AppendMenuW(menu,MF_STRING|(current==i?MF_CHECKED:0),base+i,id==Language?optionsLanguage[i]:id==Theme?optionsTheme[i]:optionsZoom[i]);
        RECT r{};GetWindowRect(app.controls[id].window,&r);int selected=int(TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTALIGN,r.right,r.bottom,0,app.window,nullptr));DestroyMenu(menu);if(selected)command(selected);return;}
    case RemoveFolder:{HMENU menu=CreatePopupMenu();for(size_t i=0;i<app.preferences.folders.size();++i)AppendMenuW(menu,MF_STRING,700+i,app.preferences.folders[i].wstring().c_str());
        RECT r{};GetWindowRect(app.controls[id].window,&r);int selected=int(TrackPopupMenu(menu,TPM_RETURNCMD,r.left,r.bottom,0,app.window,nullptr));DestroyMenu(menu);if(selected>=700&&size_t(selected-700)<app.preferences.folders.size()){app.preferences.folders.erase(app.preferences.folders.begin()+selected-700);app.saveSettings();app.rebuildLibrary();}break;}
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
    case ClearRecent:app.preferences.recent.clear();app.rebuildLibrary();app.saveSettings();break;
    case ReleasePage:ShellExecuteW(app.window,L"open",app.helpPage==2?L"https://github.com/Lyvt-Dev/PAF-Photos/blob/main/docs/formats.md":L"https://github.com/Lyvt-Dev/PAF-Photos/releases",nullptr,nullptr,SW_SHOWNORMAL);break;
    case SourcePage:ShellExecuteW(app.window,L"open",L"https://github.com/Lyvt-Dev/PAF-Photos",nullptr,nullptr,SW_SHOWNORMAL);break;
    case ErrorDetails:MessageBoxW(app.window,app.error.c_str(),tr(L"Warum kann dieses Bild nicht geöffnet werden?"),MB_OK|MB_ICONINFORMATION);break;
    }
    refresh();
}
LRESULT CALLBACK wndProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    switch(message) {
    case WM_NCCALCSIZE:
        if(wp){if(IsZoomed(window)&&(GetWindowLongW(window,GWL_STYLE)&WS_THICKFRAME)){auto parameters=reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);UINT dpi=GetDpiForWindow(window);int border=GetSystemMetricsForDpi(SM_CXSIZEFRAME,dpi)+GetSystemMetricsForDpi(SM_CXPADDEDBORDER,dpi);InflateRect(&parameters->rgrc[0],-border,-border);}return 0;}break;
    case WM_NCHITTEST:{
        POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};RECT outer{};GetWindowRect(window,&outer);UINT dpi=GetDpiForWindow(window);float scale=dpi/96.0f;
        if(!IsZoomed(window)&&(GetWindowLongW(window,GWL_STYLE)&WS_THICKFRAME)){
            int edge=GetSystemMetricsForDpi(SM_CXSIZEFRAME,dpi)+GetSystemMetricsForDpi(SM_CXPADDEDBORDER,dpi);
            bool left=point.x<outer.left+edge,right=point.x>=outer.right-edge,top=point.y<outer.top+edge,bottom=point.y>=outer.bottom-edge;
            if(top&&left)return HTTOPLEFT;
            if(top&&right)return HTTOPRIGHT;
            if(bottom&&left)return HTBOTTOMLEFT;
            if(bottom&&right)return HTBOTTOMRIGHT;
            if(left)return HTLEFT;
            if(right)return HTRIGHT;
            if(top)return HTTOP;
            if(bottom)return HTBOTTOM;
        }
        ScreenToClient(window,&point);float x=point.x/scale,y=point.y/scale;
        if(y<64){
            for(const auto& entry:app.controls){const auto& c=entry.second;if(c.visible&&x>=c.box.x&&x<c.box.x+c.box.w&&y>=c.box.y&&y<c.box.y+c.box.h)return HTCLIENT;}
            RECT search{};if(IsWindowVisible(app.search)){GetWindowRect(app.search,&search);POINT screen{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};if(PtInRect(&search,screen))return HTCLIENT;}
            return HTCAPTION;
        }return HTCLIENT;
    }
    case WM_CREATE:
        app.window=window;app.dpi=GetDpiForWindow(window);DragAcceptFiles(window,TRUE);
        app.tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        SendMessageW(app.tooltip,TTM_SETMAXTIPWIDTH,0,420);SendMessageW(app.tooltip,TTM_SETDELAYTIME,TTDT_INITIAL,450);
        app.begin();layoutControls();return 0;
    case WM_PAINT:paint(window);return 0;
    case WM_DRAWITEM:drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_COMMAND:if(LOWORD(wp)==Search&&HIWORD(wp)==EN_CHANGE){int count=GetWindowTextLengthW(app.search);std::vector<wchar_t> text(count+1);GetWindowTextW(app.search,text.data(),count+1);app.query=text.data();app.filterLibrary();refresh();return 0;}if(HIWORD(wp)==BN_CLICKED){command(LOWORD(wp));return 0;}break;
    case WM_CTLCOLOREDIT:if(reinterpret_cast<HWND>(lp)==app.search){HDC dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,ui::text.ToCOLORREF());SetBkColor(dc,ui::raised.ToCOLORREF());static HBRUSH brush=nullptr;if(brush)DeleteObject(brush);brush=CreateSolidBrush(ui::raised.ToCOLORREF());return reinterpret_cast<LRESULT>(brush);}break;
    case WM_ERASEBKGND:return 1;
    case WM_THEMECHANGED:case WM_SETTINGCHANGE:refresh();return 0;
    case WM_SIZE:if(wp!=SIZE_MINIMIZED)refresh();return 0;
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
            if(app.picture&&!app.files.empty()){app.preferences.remember(app.files[app.index]);app.rebuildLibrary();app.saveSettings();}
            refresh();
        }return 0;
    }
    case ExportDone:{
        std::optional<ExportResult> result;{std::lock_guard<std::mutex> lock(app.mutex);result=std::move(app.exported);app.exported.reset();}
        app.exporting=false;
        if(result&&result->saved){app.lastExport=result->target;app.exportNotice=true;app.notice=tr(L"Kopie gespeichert: ")+result->target.filename().wstring();app.rebuildLibrary();}
        else {app.notice=tr(L"Konvertierung fehlgeschlagen");MessageBoxW(window,tr(L"Speichern fehlgeschlagen. Wähle einen anderen Dateinamen und einen beschreibbaren Ordner. Die Originaldatei kann nicht überschrieben werden."),tr(L"Bild konvertieren"),MB_OK|MB_ICONERROR);}
        SetTimer(window,3,5000,nullptr);refresh();return 0;
    }
    case UpdateDone:refresh();return 0;
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
        if(shellNavigation()&&app.helpPage<0){auto l=viewLayout();float scale=app.dpi/96.0f,x=GET_X_LPARAM(lp)/scale,y=GET_Y_LPARAM(lp)/scale;
            auto track=scrollTrack();if(x>l.width-20&&y>track.top&&y<track.top+track.height&&track.maximum>0){
                app.scrollDragging=true;SetCapture(window);app.scrollGrab=y>=track.position&&y<=track.position+track.thumb?y-track.position:track.thumb/2;scrollDrag(y);return 0;}}

        SetFocus(window);POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};float scale=app.dpi/96.0f;auto r=viewLayout().image;
        if(app.picture&&!app.gallery&&!app.settings&&app.helpPage<0&&RectF(r.x,r.y,r.w,r.h).Contains(p.x/scale,p.y/scale)){app.dragging=true;app.drag=p;SetCapture(window);}return 0;
    }
    case WM_MOUSEMOVE:
        if(app.scrollDragging){float y=GET_Y_LPARAM(lp)/(app.dpi/96.0f);scrollDrag(y);return 0;}
        if(app.dragging) {
            POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};double scale=app.dpi/96.0;
            app.panX+=(p.x-app.drag.x)/scale;app.panY+=(p.y-app.drag.y)/scale;app.drag=p;updateZoom();InvalidateRect(window,nullptr,FALSE);
        }return 0;
    case WM_LBUTTONUP:app.scrollDragging=false;app.dragging=false;ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:app.scrollDragging=false;app.dragging=false;return 0;
    case WM_LBUTTONDBLCLK:if(app.picture&&!app.gallery&&!app.settings&&app.helpPage<0)command(app.fit?Actual:Fit);return 0;
    case WM_MOUSEWHEEL:
        if(app.helpPage<0&&(app.gallery||app.settings)){scrollWheel(GET_WHEEL_DELTA_WPARAM(wp));return 0;}
        if(app.picture&&!app.gallery&&!app.settings&&app.helpPage<0) {
            POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(window,&p);float scale=app.dpi/96.0f;
            auto r=viewLayout().image;
            if(RectF(r.x,r.y,r.w,r.h).Contains(p.x/scale,p.y/scale))zoomAt(std::pow(1.25,GET_WHEEL_DELTA_WPARAM(wp)/120.0),p.x/scale,p.y/scale);
        }return 0;
    case WM_KEYDOWN:if(shortcut(UINT(wp)))return 0;break;
    case WM_TIMER:
        if(wp==6){auto now=std::chrono::steady_clock::now();double elapsed=std::chrono::duration<double>(now-app.motionTime).count();app.motionTime=now;
            if(!scrollFrame(elapsed)){KillTimer(window,6);app.scrollAnimating=false;}return 0;}
        if(wp==5&&app.preferences.autoUpdates)checkUpdates();
        if(wp==1&&!app.busy&&app.files.size()>1&&app.helpPage<0)command(Next);
        if(wp==2&&app.busy)InvalidateRect(window,nullptr,FALSE);
        if(wp==4){bool active=false;for(auto& entry:app.controls){auto& c=entry.second;float target=app.hover==entry.first?1.0f:0.0f;if(std::abs(c.hoverProgress-target)>0.01f){c.hoverProgress=motionEnabled()?c.hoverProgress+(target-c.hoverProgress)*0.28f:target;if(std::abs(c.hoverProgress-target)<0.01f)c.hoverProgress=target;InvalidateRect(c.window,nullptr,FALSE);active|=c.hoverProgress!=target;}}if(!active)KillTimer(window,4);}
        if(wp==3){app.notice.clear();KillTimer(window,3);InvalidateRect(window,nullptr,FALSE);}
        return 0;
    case WM_CLOSE:{
        if(!app.fullscreen&&!IsIconic(window)&&!IsZoomed(window)) {
            RECT r{};GetWindowRect(window,&r);app.preferences.width=int((r.right-r.left)*96/app.dpi);app.preferences.height=int((r.bottom-r.top)*96/app.dpi);
        }
        auto update=app.updates.snapshot();
        if(update.phase==UpdatePhase::Ready&&app.installed&&(app.preferences.autoUpdates||app.installUpdateNow)){
            if(!startUpdateHelper(app.executableDirectory/L"PAFPhotos.Update.exe",update.installer,app.executableDirectory,update.sha256,app.restartAfterUpdate)){
                app.notice=tr(L"Update konnte nicht gestartet werden. Versuche es über Einstellungen erneut.");if(app.installUpdateNow){app.installUpdateNow=false;refresh();return 0;}}}
        app.saveSettings();DestroyWindow(window);return 0;}
    case WM_DESTROY:
        KillTimer(window,1);KillTimer(window,2);KillTimer(window,3);KillTimer(window,4);KillTimer(window,5);KillTimer(window,6);app.buffer.reset();app.shutdown();if(app.tooltip)DestroyWindow(app.tooltip);if(app.searchFont)DeleteObject(app.searchFont);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}

void dispatchMessage(MSG& message) {
        if(message.message==WM_KEYDOWN&&(message.hwnd==app.window||IsChild(app.window,message.hwnd))) {
            if(message.hwnd==app.search&&message.wParam!=VK_TAB){
                if(message.wParam=='O'&&GetKeyState(VK_CONTROL)<0){shortcut(UINT(message.wParam));return;}
                if(message.wParam==VK_ESCAPE){SetWindowTextW(app.search,L"");return;}
                if(message.wParam==VK_RETURN){if(app.gallery&&!app.settings&&app.filtered.size()==1)app.openPath(app.filtered.front());return;}
                TranslateMessage(&message);DispatchMessageW(&message);return;
            }
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
    auto check=[&](bool ok,const char* name){log<<(ok?"PASS ":"FAIL ")<<name<<"\n";log.flush();if(ok)++passed;return ok;};
    TempDirectory temp;if(temp.path.empty())return 2;
    auto hashFile=temp.path/L"hash.txt";{std::ofstream data(hashFile,std::ios::binary);data<<"abc";}
    bool hashCorrect=updateHash(hashFile)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    check(hashCorrect,"Updater SHA256 matches independent known vector");
    {std::ofstream data(hashFile,std::ios::binary);data<<"abd";}
    bool tamperCorrect=updateHash(hashFile)!="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"&&updateHash(temp.path/L"missing.exe").empty();
    check(tamperCorrect,"Updater detects changed and missing files");
    Com<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.out()))))return 3;
    bool success=hashCorrect&&tamperCorrect;
    auto fitsClient=[&](const Control& c,const RECT& client){RECT visible{};GetClientRect(c.window,&visible);HRGN region=CreateRectRgn(0,0,0,0);int kind=GetWindowRgn(c.window,region);
        if(kind!=ERROR)GetRgnBox(region,&visible);DeleteObject(region);RECT window{};GetWindowRect(c.window,&window);POINT origin{window.left,window.top};ScreenToClient(app.window,&origin);
        return origin.x+visible.left>=0&&origin.y+visible.top>=0&&origin.x+visible.right<=client.right&&origin.y+visible.bottom<=client.bottom;};

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
    auto outputs=temp.path/L"exports";fs::create_directory(outputs);
    Picture exportFixture;exportFixture.width=3;exportFixture.height=2;exportFixture.originalW=3;exportFixture.originalH=2;
    exportFixture.pixels={0,0,255,255, 0,255,0,255, 255,0,0,255, 255,255,0,255, 255,0,255,255, 0,255,255,255};
    for(auto format:{ExportFormat::PNG,ExportFormat::JPEG,ExportFormat::BMP,ExportFormat::TIFF,ExportFormat::WebP}){
        auto target=outputs/(L"converted"+std::wstring(exportExtension(format)));
        bool saved=exportPicture(exportFixture,temp.path/L"Bilder ü 日本語.png",target,format,1,true);
        auto converted=format==ExportFormat::WebP?embeddedLoad(target):wicLoad(target);
        success &=check(saved&&converted.image&&converted.image->width==2&&converted.image->height==3,"Convert format and bake rotation/mirror dimensions");
        if(format!=ExportFormat::JPEG)success &=check(converted.image&&converted.image->pixels[2]>240&&converted.image->pixels[1]>240&&converted.image->pixels[0]<15,"Conversion bakes correct mirrored and rotated pixels");
    }
    auto original=temp.path/L"Bilder ü 日本語.png";auto originalBytes=fs::file_size(original);
    success &=check(!exportPicture(exportFixture,original,original,ExportFormat::PNG,0,false)&&fs::file_size(original)==originalBytes,"Conversion refuses to overwrite original");
    Picture alpha;alpha.width=1;alpha.height=1;alpha.pixels={0,0,128,128};
    auto alphaPath=outputs/L"alpha.png";exportPicture(alpha,original,alphaPath,ExportFormat::PNG,0,false);auto decodedAlpha=wicLoad(alphaPath);
    success &=check(decodedAlpha.image&&decodedAlpha.image->pixels[3]>=127&&decodedAlpha.image->pixels[3]<=129,"PNG conversion preserves transparency");
    auto alphaWebP=outputs/L"alpha.webp";exportPicture(alpha,original,alphaWebP,ExportFormat::WebP,0,false);auto decodedWebPAlpha=embeddedLoad(alphaWebP);
    success &=check(decodedWebPAlpha.image&&decodedWebPAlpha.image->pixels[3]>=127&&decodedWebPAlpha.image->pixels[3]<=129,"WebP conversion preserves transparency");
    alpha.pixels={0,0,0,0};auto opaquePath=outputs/L"opaque.jpg";exportPicture(alpha,original,opaquePath,ExportFormat::JPEG,0,false);auto decodedOpaque=wicLoad(opaquePath);
    success &=check(decodedOpaque.image&&decodedOpaque.image->pixels[0]>240&&decodedOpaque.image->pixels[1]>240&&decodedOpaque.image->pixels[2]>240,"JPEG transparency uses white background");
    success &=check(copyPicture(exportFixture,1,true)&&IsClipboardFormatAvailable(CF_BITMAP),"Copy transformed image to Windows clipboard");
    fs::copy_file(temp.path/L"Bilder ü 日本語.png",temp.path/L"Bild 2.png");
    fs::copy_file(temp.path/L"Bilder ü 日本語.png",temp.path/L"Bild 10.png");
    auto invalid=temp.path/L"broken.png";{std::ofstream f(invalid);f<<"not an image";}
    success &=check(!wicLoad(invalid).image,"Reject corrupt image");
    success &=check(!wicLoad(temp.path/L"missing.png").image,"Reject missing file");
    success &=check(foto::recognized("UPPER.PNG") && !foto::recognized("image.png.exe"),"Extension registry");
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=wndProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=AppClass;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.style=CS_DBLCLKS;
    if(RegisterClassExW(&wc)) {
        HWND window=CreateWindowExW(WS_EX_COMPOSITED,AppClass,L"PAF-Photos · UI-Test",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,1180,800,nullptr,nullptr,wc.hInstance,nullptr);
        success &=check(window!=nullptr,"Create desktop window");
        if(window) {
            ShowWindow(window,SW_SHOW);
            app.openPath(temp.path/L"Bilder ü 日本語.png");
            auto pump=[&]{
                DWORD start=GetTickCount();
                while((app.busy||app.exporting) && GetTickCount()-start<5000) {
                    MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){dispatchMessage(message);}
                    Sleep(10);
                }
                UpdateWindow(window);
            };
            pump();
            success &=check(app.picture && !app.busy && app.error.empty(),"Asynchronous desktop image load");
            auto asynchronousCopy=outputs/L"background.webp";bool conversionQueued=app.exportCopy(asynchronousCopy,ExportFormat::WebP);refresh();
            success &=check(conversionQueued&&app.exporting&&!IsWindowEnabled(app.controls[Export].window),"Conversion queues in background and disables duplicate export");
            command(Mirror);pump();command(Mirror);auto asynchronousDecoded=embeddedLoad(asynchronousCopy);
            success &=check(!app.exporting&&asynchronousDecoded.image&&asynchronousDecoded.image->width==2,"Background conversion completes while controls remain usable");
            success &=check(app.exportNotice&&app.lastExport==asynchronousCopy&&IsWindowVisible(app.controls[OpenSaved].window)&&IsWindowVisible(app.controls[RevealSaved].window),"Saved copy exposes persistent destination and open/reveal buttons");
            command(OpenSaved);pump();success &=check(app.files[app.index]==asynchronousCopy&&app.picture&&!app.exportNotice,"Open saved copy navigates to actual converted file");
            app.openPath(temp.path/L"Bilder ü 日本語.png");pump();
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
            success &=check(std::wstring(accessibleName)==tr(L"Drehen"),"Native button accessible label");
            int before=app.rotation;SendMessageW(app.controls[Rotate].window,BM_CLICK,0,0);
            success &=check(app.rotation==(before+1)%4,"Native button click dispatch");
            bool previousInfo=app.info;command(Info);
            success &=check(app.info!=previousInfo && IsWindowVisible(app.controls[Reveal].window)==app.info,"Details panel toggle");
            SendMessageW(app.controls[Fit].window,WM_MOUSEMOVE,0,0);SendMessageW(window,WM_TIMER,4,0);
            success &=check(app.controls[Fit].hoverProgress>0&&app.controls[Fit].hoverProgress<=1,"Hover animation advances on native mouse input");
            app.preferences.animations=false;animateButtons();success &=check(app.controls[Fit].hoverProgress==1,"Disable motion completes hover immediately");
            app.preferences.animations=true;SendMessageW(app.controls[Fit].window,WM_MOUSELEAVE,0,0);
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
            auto preferencesFile=temp.path/L"settings.ini";Preferences saved;saved.details=true;saved.strip=false;saved.width=1110;saved.accent=RGB(160,75,235);saved.radius=0;saved.animations=false;saved.recursive=true;saved.quality=90;saved.autoUpdates=false;
            saved.remember(temp.path/L"Bilder ü 日本語.png");saved.write(preferencesFile);Preferences restored;restored.read(preferencesFile);
            success &=check(restored.details&&!restored.strip&&restored.width==1110&&restored.recent==saved.recent,"Preferences and Unicode recent paths persist");
            success &=check(restored.accent==saved.accent&&restored.radius==0&&!restored.animations&&restored.recursive&&restored.quality==90&&!restored.autoUpdates,"Color, radius, motion, recursive gallery and export quality persist");
            command(Mirror);success &=check(app.mirrored,"Mirror viewer");command(ResetView);success &=check(!app.mirrored&&app.rotation==0&&app.fit,"Reset transforms");
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
                    if(!fitsClient(entry.second,client))fits=false;
                }
                success &=check(fits,"Responsive controls stay inside window");
            }
            command(Home);success &=check(app.gallery&&!app.filtered.empty()&&IsWindowVisible(app.controls[GalleryBase].window),"Gallery with recently opened images");
            auto originalLanguage=app.preferences.language;
            app.preferences.folders={temp.path};app.rebuildLibrary();refresh();
            success &=check(app.filtered.size()>=7,"Imported folder populates gallery");
            SetWindowTextW(app.search,L"Bild 2");
            success &=check(app.filtered.size()==1&&app.filtered.front().filename()==L"Bild 2.png","Native search filters gallery");
            SetWindowTextW(app.search,L"no-such-file");success &=check(app.filtered.empty(),"Search empty state");
            SetWindowTextW(app.search,L"");command(SelectMode);command(GalleryBase);
            success &=check(app.selected.size()==1,"Native gallery selection");command(Favorite);
            success &=check(app.preferences.favorites.size()==1,"Save favorite");command(Favorites);
            success &=check(app.filtered.size()==1,"Favorites gallery filters saved photos");command(Favorite);
            success &=check(app.filtered.empty()&&app.preferences.favorites.empty(),"Remove favorite");command(SelectMode);command(Gallery);
            auto directCount=app.library.size();command(Recursive);success &=check(app.library.size()>directCount,"Recursive gallery includes nested exported files");command(Recursive);success &=check(app.library.size()==directCount,"Disable recursive gallery");
            command(Settings);success &=check(app.settings&&IsWindowVisible(app.controls[Language].window),"Settings screen controls");
            command(602);GetWindowTextW(app.controls[Settings].window,accessibleName,128);
            success &=check(foto::english&&std::wstring(accessibleName)==L"Settings"&&std::wstring(tr(L"Datei nicht gefunden"))==L"File not found","English buttons and error localization");
            command(601);GetWindowTextW(app.controls[Settings].window,accessibleName,128);
            success &=check(!foto::english&&std::wstring(accessibleName)==L"Einstellungen","German buttons switch immediately");
            command(612);success &=check(ui::background.GetR()>240,"Light theme palette");command(611);success &=check(ui::background.GetR()<50,"Dark theme palette");
            command(621);app.openPath(temp.path/L"Bild 2.png");pump();success &=check(!app.fit&&app.zoom==1,"Persisted actual-size opening preference");command(620);
            command(Settings);SendMessageW(window,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA),0);success &=check(app.settingsScroll.target>0&&app.settingsScroll.position==0,"Settings wheel queues smooth pixel movement");
            bool scrollPaint=true;
            for(int step=0;step<30;++step){SendMessageW(window,WM_MOUSEWHEEL,MAKEWPARAM(0,step<15?-WHEEL_DELTA:WHEEL_DELTA),0);scrollFrame(1.0/60);
                for(const auto& entry:app.controls)if(entry.second.visible&&entry.first!=Search){HWND button=entry.second.window;RECT area{};GetClientRect(button,&area);
                    HDC screen=GetDC(button),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,area.right,area.bottom);auto old=SelectObject(memory,bitmap);
                    PatBlt(memory,0,0,area.right,area.bottom,WHITENESS);SendMessageW(button,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT);
                    scrollPaint&=SendMessageW(button,WM_ERASEBKGND,reinterpret_cast<WPARAM>(memory),0)==1&&GetPixel(memory,area.right/2,2)!=RGB(255,255,255);
                    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(button,screen);}}
            success &=check(scrollPaint,"Rapid dark settings scroll paints native buttons without white erase or print backgrounds");
            app.settingsScroll.reset(90);refresh();auto& themeButton=app.controls[Theme];RECT clipped{};HRGN nativeClip=CreateRectRgn(0,0,0,0);GetWindowRgn(themeButton.window,nativeClip);GetRgnBox(nativeClip,&clipped);DeleteObject(nativeClip);
            success &=check(themeButton.visible&&clipped.top>0&&clipped.bottom<=themeButton.pixelBounds.bottom,"Partial settings buttons are clipped to the viewport instead of popping out");
            app.settingsScroll.reset();refresh();scrollWheel(-15);double fractionalTarget=app.settingsScroll.target;
            for(int frame=0;frame<80;++frame)scrollFrame(1.0/60);
            success &=check(fractionalTarget>0&&fractionalTarget<90&&app.settingsScroll.position==fractionalTarget,"Native precision wheel input settles at fractional pixel destination");
            app.preferences.animations=false;scrollWheel(-120);success &=check(app.settingsScroll.position==app.settingsScroll.target,"Reduced motion scrolls immediately without animation");app.preferences.animations=true;
            auto track=scrollTrack();app.scrollGrab=0;scrollDrag(track.top+track.height-track.thumb);success &=check(app.settingsScroll.position==track.maximum&&app.settingsScroll.velocity==0,"Scrollbar reaches last setting without residual spring motion");
            app.settingsScroll.reset(420);refresh();auto& radiusButton=app.controls[Radius];int originalRadius=app.preferences.radius;bool rounded=true;
            for(int radius:{4,8,12,16}){app.preferences.radius=radius;for(UINT dpi:{96u,144u,192u}){app.dpi=dpi;refresh();RECT size{};GetClientRect(radiusButton.window,&size);HDC screen=GetDC(radiusButton.window),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,size.right,size.bottom);auto old=SelectObject(memory,bitmap);
                SetFocus(radiusButton.window);SendMessageW(radiusButton.window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT);
                rounded&=GetPixel(memory,0,0)==ui::raised.ToCOLORREF()&&GetPixel(memory,size.right-1,0)==ui::raised.ToCOLORREF()&&GetPixel(memory,size.right/2,int(10*dpi/96))!=ui::surface.ToCOLORREF();
                SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(radiusButton.window,screen);}}
            app.dpi=96;app.preferences.radius=originalRadius;app.settingsScroll.reset();SetFocus(window);refresh();
            success &=check(rounded,"Raised rounded buttons and focus retain clean corners at 100/150/200 percent DPI");
            // Reused drawing surfaces must not accumulate GDI objects over a long scroll.
            for(int frame=0;frame<60;++frame){app.settingsScroll.add(frame<30?40:-40,scrollTrack().maximum);scrollFrame(1.0/60);}
            DWORD objects=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);auto began=std::chrono::steady_clock::now();
            for(int frame=0;frame<120;++frame){app.settingsScroll.add(frame<60?24:-24,scrollTrack().maximum);scrollFrame(1.0/60);}
            auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
            log<<"Scroll render average: "<<elapsed/120<<" ms per frame (test machine, not a display FPS guarantee)\n";
            success &=check(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=objects+4,"Repeated scrolling reuses drawing resources without leaking GDI objects");
            SetWindowPos(window,nullptr,0,0,780,560,SWP_NOZORDER);command(Gallery);app.tileSize=150;app.galleryScroll.reset();refresh();scrollWheel(-120);scrollFrame(1.0/60);
            success &=check(app.galleryScroll.position>0&&app.galleryScroll.position<app.galleryScroll.target,"Gallery moves by pixels rather than jumping a whole row");
            auto& tile=app.controls[GalleryBase];HRGN tileClip=CreateRectRgn(0,0,0,0);GetWindowRgn(tile.window,tileClip);GetRgnBox(tileClip,&clipped);DeleteObject(tileClip);
            success &=check(tile.visible&&clipped.top>0,"Partial gallery thumbnails stay clipped below the gallery toolbar");
            SetWindowPos(window,nullptr,0,0,1180,800,SWP_NOZORDER);command(Settings);app.settingsScroll.reset();refresh();
            command(Maximize);success &=check(IsZoomed(window),"Integrated title-bar maximize");command(Maximize);success &=check(!IsZoomed(window),"Integrated title-bar restore");
            command(Gallery);command(NavToggle);
            success &=check(app.navExpanded&&app.controls[Gallery].box.w>180,"Expanded navigation");
            command(NavToggle);
            SetFocus(app.search);MSG tab{};tab.hwnd=app.search;tab.message=WM_KEYDOWN;tab.wParam=VK_TAB;dispatchMessage(tab);
            success &=check(GetFocus()!=app.search,"Tab leaves the native search field");
            for(int page:{Gallery,Settings}){SetWindowPos(window,nullptr,0,0,780,560,SWP_NOZORDER);command(page);command(NavToggle);RECT client{};GetClientRect(window,&client);bool fits=true;float scale=app.dpi/96.0f;
                for(const auto& c:app.controls)if(c.second.visible){if(!fitsClient(c.second,client))fits=false;}
                success &=check(fits,"Small gallery/settings with expanded navigation");command(NavToggle);}
            for(UINT dpi:{96u,144u,192u}){
                RECT size{0,0,LONG(1200*dpi/96),LONG(840*dpi/96)};SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),reinterpret_cast<LPARAM>(&size));
                for(int page:{Gallery,Settings}){command(page);RECT client{};GetClientRect(window,&client);bool fits=true;float scale=dpi/96.0f;
                    for(const auto& c:app.controls)if(c.second.visible){if(!fitsClient(c.second,client))fits=false;}
                    success &=check(fits,"Gallery/settings bounds at 100/150/200 percent DPI");}
            }
            app.preferences.language=originalLanguage;app.applyLanguage();
            Preferences options;options.language=2;options.theme=2;options.tileInfo=false;options.actual=true;options.folders={temp.path};options.favorites={temp.path/L"Bild 2.png"};options.write(preferencesFile);Preferences roundtrip;roundtrip.read(preferencesFile);
            success &=check(roundtrip.language==2&&roundtrip.theme==2&&!roundtrip.tileInfo&&roundtrip.actual&&roundtrip.folders==options.folders&&roundtrip.favorites==options.favorites,"Language, theme, library and favorites persist");
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
int codecSelfTest(const fs::path& directory,const fs::path& report){
    std::ofstream log(report);bool success=true;int passed=0;
    auto check=[&](bool ok,const std::string& name){log<<(ok?"PASS ":"FAIL ")<<name<<"\n";log.flush();if(ok)++passed;else success=false;};
    check(!magickPath().empty(),"Portable codec discovery");
    for(const auto* extension:{L"avif",L"heic",L"jxl",L"jp2",L"exr",L"dds",L"pcx"}){
        std::wstring suffix(extension);std::string name(suffix.begin(),suffix.end());
        auto result=load(directory/(L"red."+suffix));check(bool(result.image),name+" actual decode");
        if(result.image){const auto& image=*result.image;bool red=false;
            for(size_t i=0;i<image.pixels.size();i+=4)if(image.pixels[i+2]>225&&image.pixels[i]<25&&image.pixels[i+1]<25){red=true;break;}
            check(red,name+" decoded red pixels");
            auto preview=thumbnail(directory/(L"red."+suffix));check(preview&&preview->width<=400&&preview->height<=400,name+" gallery thumbnail");
        }else {std::string message(result.error.begin(),result.error.end());log<<message<<"\n";}
    }
    log<<passed<<" codec checks passed\n";return success?0:1;
}

int updateSelfTest(const fs::path& report){
    std::ofstream log(report);TempDirectory temp;UpdateService service;
    if(temp.path.empty()||!service.start(nullptr,0,"0.0.0",temp.path,true))return 2;
    DWORD began=GetTickCount();UpdateSnapshot state;
    do{Sleep(50);state=service.snapshot();}while(state.phase!=UpdatePhase::Ready&&state.phase!=UpdatePhase::Failed&&GetTickCount()-began<120000);
    service.stop();bool good=state.phase==UpdatePhase::Ready&&state.version==FOTO_VERSION_STRING&&updateHash(state.installer)==state.sha256;
    log<<(good?"PASS ":"FAIL ")<<"Published HTTPS manifest, version comparison, actual setup download and SHA256 verification\n";
    log<<"Version "<<state.version<<"\nSHA256 "<<state.sha256<<"\n";return good?0:1;
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    GdiplusStartupInput startup;ULONG_PTR token=0;
    if(GdiplusStartup(&token,&startup,nullptr)!=Ok){if(SUCCEEDED(co))CoUninitialize();return 1;}
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argc>2&&std::wstring(args[1])==L"--update-test"){int result=updateSelfTest(args[2]);LocalFree(args);GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return result;}
    if(argc>3&&std::wstring(args[1])==L"--codec-test"){foto::english=true;int result=codecSelfTest(args[2],args[3]);LocalFree(args);ui::releaseBrand();GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return result;}
    if(argc>1&&std::wstring(args[1])==L"--self-test") {
        int result=selfTest(argc>2?fs::path(args[2]):fs::path(L"self-test.txt"));LocalFree(args);ui::releaseBrand();GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return result;
    }
    app.initSettings();wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);app.executableDirectory=fs::path(executable).parent_path();app.installed=installedApp(app.executableDirectory);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_DBLCLKS;wc.lpfnWndProc=wndProc;wc.hInstance=instance;wc.lpszClassName=AppClass;
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(2));wc.hIconSm=wc.hIcon;
    if(!RegisterClassExW(&wc)){LocalFree(args);ui::releaseBrand();GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return 1;}
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    UINT dpi=GetDpiForSystem();int width=std::min(int(app.preferences.width*dpi/96),int(work.right-work.left)),height=std::min(int(app.preferences.height*dpi/96),int(work.bottom-work.top));
    HWND window=CreateWindowExW(WS_EX_COMPOSITED,AppClass,AppTitle,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,nullptr,nullptr,instance,nullptr);
    if(!window){LocalFree(args);ui::releaseBrand();GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return 1;}
    BOOL dark=TRUE;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
    ui::palette();
    ShowWindow(window,show);UpdateWindow(window);
    if(argc>1)app.openPath(args[1]);
    LocalFree(args);
    SetTimer(window,5,24u*60*60*1000,nullptr);if(app.preferences.autoUpdates)checkUpdates();
    MSG message{};int status;
    while((status=GetMessageW(&message,nullptr,0,0))>0) {
        dispatchMessage(message);
    }
    ui::releaseBrand();GdiplusShutdown(token);if(SUCCEEDED(co))CoUninitialize();return status<0?1:int(message.wParam);
}
