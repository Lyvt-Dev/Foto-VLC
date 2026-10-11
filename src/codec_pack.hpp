#pragma once
#include "updater.hpp"
enum class CodecPhase{Idle,Downloading,Installing,Ready,Failed};
struct CodecSnapshot{CodecPhase phase=CodecPhase::Idle;std::string version;};
fs::path userCodecDirectory();
class CodecPack{
 UpdateService http;std::thread worker;std::mutex mutex;CodecSnapshot state;std::atomic<bool> running{false},cancelled{false};
public:
 ~CodecPack(){stop();}
 CodecSnapshot snapshot(){std::lock_guard<std::mutex> lock(mutex);return state;}
 bool start(HWND window,UINT notification,const std::string& version,const fs::path& directory={});
 void stop(){cancelled=true;http.stop();if(worker.joinable())worker.join();running=false;}
};
