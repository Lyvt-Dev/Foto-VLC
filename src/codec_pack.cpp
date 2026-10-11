#include "codec_pack.hpp"
#include "core.hpp"
#include <shlobj.h>
#include <fstream>
fs::path userCodecDirectory(){PWSTR local=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))return {};fs::path path=fs::path(local)/L"PAFPhotos"/L"codecs";CoTaskMemFree(local);return path;}
static std::wstring literal(const fs::path& path){std::wstring result=L"'";for(auto c:path.wstring()){result+=c;if(c==L'\'')result+=c;}return result+L"'";}
static bool extract(const fs::path& archive,const fs::path& directory,std::atomic<bool>& cancelled){
 wchar_t system[32768]{};if(!GetSystemDirectoryW(system,32768))return false;fs::path exe=fs::path(system)/L"WindowsPowerShell"/L"v1.0"/L"powershell.exe";
 // Execute only our fixed extraction program; literal paths are escaped for PowerShell.
 auto script=L"$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath "+literal(archive)+L" -DestinationPath "+literal(directory)+L" -Force";
 auto command=foto::quote(exe.wstring())+L" -NoLogo -NoProfile -NonInteractive -Command "+foto::quote(script);
 STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
 if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))return false;
 DWORD waited=0,exit=1;while(!cancelled&&waited<120000){if(WaitForSingleObject(process.hProcess,100)==WAIT_OBJECT_0){GetExitCodeProcess(process.hProcess,&exit);break;}waited+=100;}
 if(cancelled||waited>=120000){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,2000);}CloseHandle(process.hThread);CloseHandle(process.hProcess);return !cancelled&&exit==0;
}
bool CodecPack::start(HWND window,UINT notification,const std::string& version,const fs::path& directory){
 if(running.exchange(true))return false;if(worker.joinable())worker.join();cancelled=false;http.prepareDownload();
 auto publish=[this,window,notification](CodecPhase phase,const std::string& installed=""){std::lock_guard<std::mutex> lock(mutex);state={phase,installed};if(window&&!cancelled)PostMessageW(window,notification,0,0);};
 publish(CodecPhase::Downloading);
 worker=std::thread([this,directory,version,publish]{fs::path stage,archive;std::error_code error;
  try{auto destination=directory.empty()?userCodecDirectory():directory;if(destination.empty())throw 1;
   std::wstring wide(version.begin(),version.end());auto base=L"https://github.com/Lyvt-Dev/PAF-Photos/releases/download/v"+wide+L"/";std::string json;
   if(!http.download(base+L"decoders.json",{},8192,&json))throw 1;auto manifest=paf::manifest(json);if(!manifest||manifest->version!=version)throw 1;
   fs::create_directories(destination.parent_path(),error);if(error)throw 1;
   auto unique=std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());stage=destination.parent_path()/(L"codec-stage-"+unique);archive=destination.parent_path()/(L"codec-"+unique+L".zip");
   if(!http.download(base+L"PAFPhotos-Decoders-x64.zip",archive,size_t(manifest->size))||fs::file_size(archive)!=manifest->size||updateHash(archive)!=manifest->sha256||cancelled)throw 1;
   publish(CodecPhase::Installing);if(!extract(archive,stage,cancelled)||!fs::is_regular_file(stage/L"magick.exe")||!fs::is_regular_file(stage/L"SBOM.json"))throw 1;
   // Activate only a completely extracted, SHA256-verified package.
   auto backup=destination.parent_path()/(L"codec-backup-"+unique);bool previous=fs::exists(destination);
   if(previous&&!MoveFileExW(destination.c_str(),backup.c_str(),MOVEFILE_WRITE_THROUGH))throw 1;
   if(!MoveFileExW(stage.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)){if(previous)MoveFileExW(backup.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH);throw 1;}
   if(previous)fs::remove_all(backup,error);fs::remove(archive,error);publish(CodecPhase::Ready,version);
  }catch(...){if(!stage.empty())fs::remove_all(stage,error);if(!archive.empty())fs::remove(archive,error);if(!cancelled)publish(CodecPhase::Failed);}
  running=false;
 });return true;
}
