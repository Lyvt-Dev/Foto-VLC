#include "decoder_catalog.hpp"
#include "core.hpp"
#include "i18n.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <cwctype>
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>
std::wstring decoderCatalog(){
 std::wstring text=tr(L"Eingebaut (keine Installation nötig):")+std::wstring(L"\r\nPNG JPEG BMP DIB GIF TIFF ICO WebP SVG PSD TGA QOI PCX DDS PNM PAM SGI XBM HDR\r\n\r\n");
 Com<IWICImagingFactory> factory;Com<IEnumUnknown> enumeration;
 if(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.out())))&&SUCCEEDED(factory->CreateComponentEnumerator(WICDecoder,WICComponentEnumerateDefault,enumeration.out()))){
  text+=tr(L"Auf diesem PC registrierte Windows-Decoder:")+std::wstring(L"\r\n");Com<IUnknown> entry;
  while(enumeration->Next(1,entry.out(),nullptr)==S_OK){Com<IWICBitmapCodecInfo> info;if(SUCCEEDED(entry->QueryInterface(IID_PPV_ARGS(info.out())))){wchar_t name[512]{},extensions[2048]{};UINT used=0;info->GetFriendlyName(512,name,&used);info->GetFileExtensions(2048,extensions,&used);text+=std::wstring(name)+L"  "+extensions+L"\r\n";}}
 }
 text+=L"\r\n"+std::wstring(tr(L"Erkannte Endungen (Unterstützung abhängig vom Decoder):"))+L"\r\n";int column=0;for(const auto& extension:foto::extensions()){text+=std::wstring(extension.begin(),extension.end())+L"  ";if(++column%10==0)text+=L"\r\n";}text+=L"\r\n";
 auto executable=magickPath();if(executable.empty())return text+L"\r\n"+tr(L"Installiere das Decoder-Paket, um alle verfügbaren ImageMagick-Leser anzuzeigen.");
 TempDirectory temp;if(temp.path.empty())return text;auto file=temp.path/L"formats.txt";
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE output=CreateFileW(file.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,nullptr);if(output==INVALID_HANDLE_VALUE)return text;
 STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=si.hStdError=output;HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);si.hStdInput=input;PROCESS_INFORMATION process{};
 std::wstring command=foto::quote(executable)+L" -list format";
 bool ok=CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,temp.path.c_str(),&si,&process)!=FALSE;
 if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);if(ok){if(WaitForSingleObject(process.hProcess,5000)!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,1000);}CloseHandle(process.hThread);CloseHandle(process.hProcess);}CloseHandle(output);
 std::error_code error;auto bytes=fs::file_size(file,error);if(!ok||error||bytes>2*1024*1024)return text+L"\r\n"+tr(L"Decoder-Liste konnte nicht gelesen werden.");
 std::ifstream stream(file,std::ios::binary);std::string data((std::istreambuf_iterator<char>(stream)),{});int count=MultiByteToWideChar(CP_UTF8,0,data.data(),int(data.size()),nullptr,0);std::wstring decoded(count,L' ');MultiByteToWideChar(CP_UTF8,0,data.data(),int(data.size()),decoded.data(),count);
 text+=L"\r\n"+std::wstring(tr(L"Alle ImageMagick-Formate (r = Lesen, w = Schreiben; Delegates können fehlen):"))+L"\r\n";
 for(wchar_t c:decoded){if(c==L'\n')text+=L'\r';text+=c;}return text;
}
namespace {constexpr UINT Loaded=WM_APP+177;struct Catalog{HWND window{},search{},body{};COLORREF background{},foreground{};HBRUSH brush{};HFONT font{};std::wstring all;std::thread worker;bool done=false;};
void filter(Catalog& c){wchar_t query[256]{};GetWindowTextW(c.search,query,256);std::wstring needle=query;for(auto& x:needle)x=std::towlower(x);std::wstring shown;std::wistringstream lines(c.all);std::wstring line;while(std::getline(lines,line)){auto lower=line;for(auto& x:lower)x=std::towlower(x);if(needle.empty()||lower.find(needle)!=std::wstring::npos)shown+=line+L"\n";}SetWindowTextW(c.body,shown.c_str());}
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wp,LPARAM lp){auto c=reinterpret_cast<Catalog*>(GetWindowLongPtrW(window,GWLP_USERDATA));if(message==WM_NCCREATE){c=reinterpret_cast<Catalog*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);c->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(c));}if(!c)return DefWindowProcW(window,message,wp,lp);
 switch(message){case WM_CREATE:{c->brush=CreateSolidBrush(c->background);c->font=CreateFontW(-MulDiv(14,GetDpiForWindow(window),96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");c->search=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,1,1,window,(HMENU)1,nullptr,nullptr);SendMessageW(c->search,EM_SETCUEBANNER,TRUE,(LPARAM)tr(L"Format oder Decoder suchen …"));c->body=CreateWindowExW(0,L"EDIT",tr(L"Decoder werden gelesen …"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,0,0,1,1,window,(HMENU)2,nullptr,nullptr);for(auto child:{c->search,c->body}){SetWindowTheme(child,L"DarkMode_Explorer",nullptr);SendMessageW(child,WM_SETFONT,(WPARAM)c->font,TRUE);}c->worker=std::thread([c]{CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);c->all=decoderCatalog();CoUninitialize();PostMessageW(c->window,Loaded,0,0);});return 0;}
 case WM_SIZE:{float scale=GetDpiForWindow(window)/96.0f;int pad=int(20*scale),top=int(54*scale);MoveWindow(c->search,pad,pad,LOWORD(lp)-2*pad,int(30*scale),TRUE);MoveWindow(c->body,pad,pad+top,LOWORD(lp)-2*pad,std::max(1,int(HIWORD(lp))-top-2*pad),TRUE);return 0;}
 case Loaded:if(c->worker.joinable())c->worker.join();filter(*c);return 0;
 case WM_COMMAND:if(LOWORD(wp)==1&&HIWORD(wp)==EN_CHANGE&&!c->worker.joinable())filter(*c);return 0;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:SetTextColor((HDC)wp,c->foreground);SetBkColor((HDC)wp,c->background);return (LRESULT)c->brush;
 case WM_ERASEBKGND:{RECT r{};GetClientRect(window,&r);FillRect((HDC)wp,&r,c->brush);return 1;}
 case WM_CLOSE:DestroyWindow(window);return 0;case WM_DESTROY:if(c->worker.joinable())c->worker.join();DeleteObject(c->brush);DeleteObject(c->font);c->done=true;return 0;
 }return DefWindowProcW(window,message,wp,lp);}
}
void showDecoderCatalog(HWND owner,COLORREF background,COLORREF foreground){Catalog state;state.background=background;state.foreground=foreground;WNDCLASSEXW cls{sizeof(cls)};cls.hInstance=GetModuleHandleW(nullptr);cls.hIcon=cls.hIconSm=LoadIconW(cls.hInstance,MAKEINTRESOURCEW(2));cls.lpszClassName=L"PAFPhotos.DecoderCatalog";cls.lpfnWndProc=procedure;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassExW(&cls);RECT r{};GetWindowRect(owner,&r);HWND window=CreateWindowExW(0,cls.lpszClassName,tr(L"Alle Decoder und Formate"),WS_OVERLAPPEDWINDOW,r.left+40,r.top+40,820,620,owner,nullptr,cls.hInstance,&state);if(!window)return;BOOL dark=GetRValue(background)+GetGValue(background)+GetBValue(background)<384;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));EnableWindow(owner,FALSE);ShowWindow(window,SW_SHOW);MSG message{};while(!state.done&&GetMessageW(&message,nullptr,0,0)>0){if(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE){SendMessageW(window,WM_CLOSE,0,0);continue;}if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}EnableWindow(owner,TRUE);SetForegroundWindow(owner);}
