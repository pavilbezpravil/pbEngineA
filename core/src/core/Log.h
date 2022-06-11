#pragma once


#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"


class Log {
public:
   static void Init();

   static std::shared_ptr<spdlog::logger>& GetLogger() { return sLogger; }
private:
   static std::shared_ptr<spdlog::logger> sLogger;
};

// Core Logging Macros
#define TRACE(...)	Log::GetLogger()->trace(__VA_ARGS__)
#define INFO(...)	Log::GetLogger()->info(__VA_ARGS__)
#define WARN(...)	Log::GetLogger()->warn(__VA_ARGS__)
#define ERROR(...)	Log::GetLogger()->error(__VA_ARGS__)
#define FATAL(...)	Log::GetLogger()->critical(__VA_ARGS__)
