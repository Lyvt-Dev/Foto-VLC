#include "resize_dialog.hpp"
#include "i18n.hpp"
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <objidl.h>
#include <gdiplus.h>
#include <uxtheme.h>
#include <dwmapi.h>
namespace paf { namespace {
struct State {unsigned originalW,originalH,width,height;COLORREF background,foreground;HBRUSH brush=nullptr,editBrush=nullptr;bool changing=false,accepted=false,testing=false,testPassed=false;int radius=8;};
COLORREF lifted(COLORREF c,int amount){if(GetRValue(c)+GetGValue(c)+GetBValue(c)>384)amount=-amount;return RGB(std::clamp(int(GetRValue(c))+amount,0,255),std::clamp(int(GetGValue(c))+amount,0,255),std::clamp(int(GetBValue(c))+amount,0,255));}
bool valid(unsigned w,unsigned h){return w&&h&&w<=16384&&h<=16384&&std::uint64_t(w)*h<=32000000;}
unsigned value(HWND dialog,int id){BOOL translated=FALSE;UINT result=GetDlgItemInt(dialog,id,&translated,FALSE);return translated?result:0;}
void roundedPath(Gdiplus::GraphicsPath& path,float x,float y,float w,float h,float radius){float d=std::min(radius*2,std::min(w,h));if(d<=0){path.AddRectangle(Gdiplus::RectF(x,y,w,h));return;}path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();}
void update(HWND dialog,State& s,int changed){if(s.changing)return;s.changing=true;
 unsigned w=value(dialog,104),h=value(dialog,105);
 if(IsDlgButtonChecked(dialog,106)==BST_CHECKED&&changed&&(changed==104?w:h)){
  if(changed==104){h=unsigned(std::clamp(std::round(double(w)*s.originalH/s.originalW),1.0,16385.0));SetDlgItemInt(dialog,105,h,FALSE);}
  else{w=unsigned(std::clamp(std::round(double(h)*s.originalW/s.originalH),1.0,16385.0));SetDlgItemInt(dialog,104,w,FALSE);}
 }
 s.width=w;s.height=h;EnableWindow(GetDlgItem(dialog,IDOK),valid(w,h));
 auto preview=std::wstring(tr(L"Neue Größe: "))+std::to_wstring(w)+L" × "+std::to_wstring(h)+L" px";SetDlgItemTextW(dialog,107,preview.c_str());
 SetDlgItemTextW(dialog,108,valid(w,h)?tr(L"Das Original bleibt unverändert. Speichere anschließend eine Kopie."):tr(L"Pixelmaße von 1 bis 16384, maximal 32 Megapixel."));s.changing=false;
}
INT_PTR CALLBACK procedure(HWND dialog,UINT message,WPARAM wp,LPARAM lp){auto s=reinterpret_cast<State*>(GetWindowLongPtrW(dialog,DWLP_USER));
 if(message==WM_INITDIALOG){s=reinterpret_cast<State*>(lp);SetWindowLongPtrW(dialog,DWLP_USER,lp);s->brush=CreateSolidBrush(s->background);s->editBrush=CreateSolidBrush(lifted(s->background,10));SetWindowTheme(GetDlgItem(dialog,106),L"",L"");SetWindowTextW(dialog,tr(L"Bildgröße ändern"));BOOL dark=GetRValue(s->background)+GetGValue(s->background)+GetBValue(s->background)<384;DwmSetWindowAttribute(dialog,20,&dark,sizeof(dark));
  SetDlgItemTextW(dialog,102,tr(L"Breite (Pixel)"));SetDlgItemTextW(dialog,103,tr(L"Höhe (Pixel)"));SetDlgItemTextW(dialog,106,tr(L"Seitenverhältnis beibehalten"));SetDlgItemTextW(dialog,IDOK,tr(L"Anwenden"));SetDlgItemTextW(dialog,IDCANCEL,tr(L"Abbrechen"));
  auto original=std::wstring(tr(L"Original: "))+std::to_wstring(s->originalW)+L" × "+std::to_wstring(s->originalH)+L" px";SetDlgItemTextW(dialog,101,original.c_str());
  SendDlgItemMessageW(dialog,104,EM_SETLIMITTEXT,5,0);SendDlgItemMessageW(dialog,105,EM_SETLIMITTEXT,5,0);s->changing=true;SetDlgItemInt(dialog,104,s->width,FALSE);SetDlgItemInt(dialog,105,s->height,FALSE);CheckDlgButton(dialog,106,BST_CHECKED);s->changing=false;update(dialog,*s,0);
  RECT owner{},bounds{};GetWindowRect(GetParent(dialog),&owner);GetWindowRect(dialog,&bounds);SetWindowPos(dialog,nullptr,owner.left+(owner.right-owner.left-bounds.right+bounds.left)/2,owner.top+(owner.bottom-owner.top-bounds.bottom+bounds.top)/2,0,0,SWP_NOSIZE|SWP_NOZORDER);
  if(s->testing)PostMessageW(dialog,WM_APP,0,0);return TRUE;
 }
 if(!s)return FALSE;
 switch(message){
 case WM_APP:if(s->testing){SetDlgItemInt(dialog,104,800,FALSE);bool locked=s->height==600;CheckDlgButton(dialog,106,BST_UNCHECKED);SetDlgItemInt(dialog,105,333,FALSE);bool independent=s->height==333;SetDlgItemInt(dialog,104,0,FALSE);bool rejected=!IsWindowEnabled(GetDlgItem(dialog,IDOK));SetDlgItemInt(dialog,104,20000,FALSE);rejected=rejected&&!IsWindowEnabled(GetDlgItem(dialog,IDOK));SetDlgItemInt(dialog,104,8000,FALSE);SetDlgItemInt(dialog,105,8000,FALSE);rejected=rejected&&!IsWindowEnabled(GetDlgItem(dialog,IDOK));SetDlgItemInt(dialog,104,900,FALSE);SetDlgItemInt(dialog,105,333,FALSE);s->testPassed=locked&&independent&&rejected&&IsWindowEnabled(GetDlgItem(dialog,IDOK));SendMessageW(dialog,WM_COMMAND,IDOK,0);return TRUE;}break;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(dialog,&ps);RECT area{};GetClientRect(dialog,&area);FillRect(dc,&area,s->brush);{Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float scale=GetDpiForWindow(dialog)/96.0f;for(int id:{104,105}){RECT r{};GetWindowRect(GetDlgItem(dialog,id),&r);MapWindowPoints(nullptr,dialog,reinterpret_cast<POINT*>(&r),2);InflateRect(&r,int(6*scale),int(6*scale));Gdiplus::GraphicsPath path;roundedPath(path,float(r.left),float(r.top),float(r.right-r.left),float(r.bottom-r.top),s->radius*scale);Gdiplus::SolidBrush fill(Gdiplus::Color(255,GetRValue(lifted(s->background,10)),GetGValue(lifted(s->background,10)),GetBValue(lifted(s->background,10))));g.FillPath(&fill,&path);Gdiplus::Pen edge(Gdiplus::Color(255,85,90,98),1);g.DrawPath(&edge,&path);}}EndPaint(dialog,&ps);return TRUE;}
 case WM_DRAWITEM:{auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item->CtlID!=IDOK&&item->CtlID!=IDCANCEL)break;
  FillRect(item->hDC,&item->rcItem,s->brush);Gdiplus::Graphics g(item->hDC);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);auto r=item->rcItem;float x=float(r.left)+1,y=float(r.top)+1,w=float(r.right-r.left)-2,h=float(r.bottom-r.top)-3;Gdiplus::GraphicsPath path;roundedPath(path,x,y,w,h,s->radius*GetDpiForWindow(dialog)/96.0f);
  bool disabled=item->itemState&ODS_DISABLED;int lift=(item->itemState&ODS_SELECTED)?12:22;Gdiplus::SolidBrush fill(Gdiplus::Color(255,GetRValue(lifted(s->background,lift)),GetGValue(lifted(s->background,lift)),GetBValue(lifted(s->background,lift))));g.FillPath(&fill,&path);Gdiplus::Pen edge(Gdiplus::Color(255,85,90,98),1);g.DrawPath(&edge,&path);
  wchar_t caption[64]{};GetWindowTextW(item->hwndItem,caption,64);g.Flush(Gdiplus::FlushIntentionSync);SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,disabled?RGB(125,125,125):s->foreground);auto font=SelectObject(item->hDC,reinterpret_cast<HFONT>(SendMessageW(item->hwndItem,WM_GETFONT,0,0)));DrawTextW(item->hDC,caption,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(item->hDC,font);if(item->itemState&ODS_FOCUS){InflateRect(&r,-5,-5);DrawFocusRect(item->hDC,&r);}return TRUE;}
 case WM_CTLCOLOREDIT:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,s->foreground);SetBkColor(dc,lifted(s->background,10));return reinterpret_cast<INT_PTR>(s->editBrush);}
 case WM_CTLCOLORDLG:return reinterpret_cast<INT_PTR>(s->brush);
 case WM_CTLCOLORSTATIC:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,s->foreground);SetBkColor(dc,s->background);return reinterpret_cast<INT_PTR>(s->brush);}
 case WM_COMMAND:{int id=LOWORD(wp);if((id==104||id==105)&&HIWORD(wp)==EN_CHANGE){update(dialog,*s,id);return TRUE;}if(id==106){update(dialog,*s,104);return TRUE;}
  if(id==IDOK){update(dialog,*s,0);if(!valid(s->width,s->height))return TRUE;s->accepted=true;EndDialog(dialog,IDOK);return TRUE;}if(id==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}break;}
 case WM_CLOSE:EndDialog(dialog,IDCANCEL);return TRUE;
 case WM_DESTROY:if(s->editBrush){DeleteObject(s->editBrush);s->editBrush=nullptr;}if(s->brush){DeleteObject(s->brush);s->brush=nullptr;}break;
 }return FALSE;
}
}
std::optional<std::pair<unsigned,unsigned>> showResizeDialog(HWND owner,unsigned width,unsigned height,COLORREF background,COLORREF foreground,int radius){if(!width||!height)return {};State state{width,height,width,height,background,foreground};state.radius=std::clamp(radius,0,16);auto result=DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(200),owner,procedure,reinterpret_cast<LPARAM>(&state));if(result==IDOK&&state.accepted)return std::pair<unsigned,unsigned>{state.width,state.height};return {};}
bool resizeDialogSelfTest(HWND owner){State state{400,300,400,300,RGB(32,35,40),RGB(240,240,240)};state.testing=true;DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(200),owner,procedure,reinterpret_cast<LPARAM>(&state));return state.testPassed&&state.accepted&&state.width==900&&state.height==333;}
}
