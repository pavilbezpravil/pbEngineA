#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#include "core/Core.h"

namespace pbe {

   class CORE_API Log {
   public:
      static void Init();

      static std::shared_ptr<spdlog::logger>& GetLogger() { return sLogger; }
   private:
      static std::shared_ptr<spdlog::logger> sLogger;
   };

#ifdef ERROR
#undef ERROR
#endif

   // Core Logging Macros
#define TRACE(...)	pbe::Log::GetLogger()->trace(__VA_ARGS__)
#define INFO(...)	   pbe::Log::GetLogger()->info(__VA_ARGS__)
#define WARN(...)	   pbe::Log::GetLogger()->warn(__VA_ARGS__)
#define ERROR(...)	pbe::Log::GetLogger()->error(__VA_ARGS__)
#define FATAL(...)	pbe::Log::GetLogger()->critical(__VA_ARGS__)

}
