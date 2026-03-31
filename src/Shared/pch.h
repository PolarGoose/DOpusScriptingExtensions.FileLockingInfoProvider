#pragma once

#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#include <Windows.h>
#include <winternl.h>
#include <wtypes.h>
#include <Psapi.h>
#include <aclapi.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlsafe.h>
#include <atlstr.h>

#include <comdef.h>

#include <string>
#include <thread>
#include <sstream>
#include <format>
#include <utility>
#include <filesystem>
#include <regex>
#include <map>
#include <ranges>
#include <generator>

#include <absl/synchronization/notification.h>

#include <grpcpp/grpcpp.h>
#include <google/protobuf/empty.pb.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/dll.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/locale.hpp>
#include <boost/preprocessor.hpp>
#include <boost/noncopyable.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/stopwatch.h>
