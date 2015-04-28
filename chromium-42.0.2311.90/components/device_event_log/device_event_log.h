// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_EVENT_LOG_DEVICE_EVENT_LOG_H_
#define COMPONENTS_DEVICE_EVENT_LOG_DEVICE_EVENT_LOG_H_

#include <cstring>
#include <sstream>

#include "base/basictypes.h"
#include "base/timer/elapsed_timer.h"
#include "components/device_event_log/device_event_log_export.h"

// These macros can be used to log device related events.
// The following values should be used for |level| in these macros:
//  ERROR Unexpected events, or device level failures. Use sparingly.
//  USER  Events initiated directly by a user (or Chrome) action.
//  EVENT Default event type.
//  DEBUG Debugging details that are usually not interesting.
// Examples:
//  NET_LOG(EVENT) << "NetworkState Changed " << name << ": " << state;
//  POWER_LOG(USER) << "Suspend requested";

#define NET_LOG(level)                             \
  DEVICE_LOG(::device_event_log::LOG_TYPE_NETWORK, \
             ::device_event_log::LOG_LEVEL_##level)
#define POWER_LOG(level)                         \
  DEVICE_LOG(::device_event_log::LOG_TYPE_POWER, \
             ::device_event_log::LOG_LEVEL_##level)
#define LOGIN_LOG(level)                         \
  DEVICE_LOG(::device_event_log::LOG_TYPE_LOGIN, \
             ::device_event_log::LOG_LEVEL_##level)

// Generally prefer the above macros unless |type| or  |level| is not constant.

#define DEVICE_LOG(type, level)                                            \
  ::device_event_log::internal::DeviceEventLogInstance(__FILE__, __LINE__, \
                                                       type, level).stream()

// Declare {Type_LOG_IF_SLOW() at the top of a method to log slow methods
// where "slow" is defined by kSlowMethodThresholdMs in the .cc file.
#define SCOPED_NET_LOG_IF_SLOW() \
  SCOPED_DEVICE_LOG_IF_SLOW(::device_event_log::LOG_TYPE_NETWORK)

// Generally prefer the above macros unless |type| is not constant.

#define SCOPED_DEVICE_LOG_IF_SLOW(type)               \
  ::device_event_log::internal::ScopedDeviceLogIfSlow \
      scoped_device_log_if_slow(type, __FILE__, __func__)

namespace device_event_log {

// Used to specify the type of event. NOTE: Be sure to update LogTypeFromString
// and GetLogTypeString when adding entries to this enum. Also consider
// updating chrome://device-log (see device_log_ui.cc).
enum LogType {
  // Shill / network configuration related events.
  LOG_TYPE_NETWORK,
  // Power manager related events.
  LOG_TYPE_POWER,
  // Login related events.
  LOG_TYPE_LOGIN,
  // Used internally
  LOG_TYPE_UNKNOWN
};

// Used to specify the detail level for logging. In GetAsString, used to
// specify the maximum detail level (i.e. EVENT will include USER and ERROR).
// See top-level comment for guidelines for each type.
enum LogLevel {
  LOG_LEVEL_ERROR = 0,
  LOG_LEVEL_USER = 1,
  LOG_LEVEL_EVENT = 2,
  LOG_LEVEL_DEBUG = 3
};

// Used to specify which order to output event entries in GetAsString.
enum StringOrder { OLDEST_FIRST, NEWEST_FIRST };

// Initializes / shuts down device event logging. If |max_entries| = 0 the
// default value will be used.
DEVICE_EVENT_LOG_EXPORT void Initialize(size_t max_entries);
DEVICE_EVENT_LOG_EXPORT void Shutdown();

// If the global instance is initialized, adds an entry to it. Regardless of
// whether the global instance was intitialzed, this logs the event to
// LOG(ERROR) if |type| = ERROR or VLOG(1) otherwise.
DEVICE_EVENT_LOG_EXPORT void AddEntry(const char* file,
                                      int line,
                                      LogType type,
                                      LogLevel level,
                                      const std::string& event);

// For backwards compatibility with network_event_log. Combines |event| and
// |description| and calls AddEntry().
DEVICE_EVENT_LOG_EXPORT void AddEntryWithDescription(
    const char* file,
    int line,
    LogType type,
    LogLevel level,
    const std::string& event,
    const std::string& description);

// Outputs the log to a formatted string.
// |order| determines which order to output the events.
// |format| is a comma-separated string that determines which elements to show.
//  e.g. "time,desc". Note: order of the strings does not affect the output.
//  "time" - Include a timestamp.
//  "file" - Include file and line number.
//  "type" - Include the event type.
//  "html" - Include html tags.
//  "json" - Return JSON format dictionaries containing entries for timestamp,
//           level, type, file, and event.
// |types| lists the types included in the output. Prepend "non-" to disclude
//  a type. e.g. "network,login" or "non-network". Use an empty string for
//  all types.
// |max_level| determines the maximum log level to be included in the output.
// |max_events| limits how many events are output if > 0, otherwise all events
//  are included.
DEVICE_EVENT_LOG_EXPORT std::string GetAsString(StringOrder order,
                                                const std::string& format,
                                                const std::string& types,
                                                LogLevel max_level,
                                                size_t max_events);

DEVICE_EVENT_LOG_EXPORT extern const LogLevel kDefaultLogLevel;

namespace internal {

// Implementation class for DEVICE_LOG macros. Provides a stream for creating
// a log string and adds the event using device_event_log::AddEntry on
// destruction.
class DEVICE_EVENT_LOG_EXPORT DeviceEventLogInstance {
 public:
  DeviceEventLogInstance(const char* file,
                         int line,
                         device_event_log::LogType type,
                         device_event_log::LogLevel level);
  ~DeviceEventLogInstance();

  std::ostream& stream() { return stream_; }

 private:
  const char* file_;
  const int line_;
  device_event_log::LogType type_;
  device_event_log::LogLevel level_;
  std::ostringstream stream_;

  DISALLOW_COPY_AND_ASSIGN(DeviceEventLogInstance);
};

// Implementation class for SCOPED_LOG_IF_SLOW macros. Tests the elapsed time on
// destruction and adds a Debug or Error log entry if it exceeds the
// corresponding expected maximum elapsed time.
class DEVICE_EVENT_LOG_EXPORT ScopedDeviceLogIfSlow {
 public:
  ScopedDeviceLogIfSlow(LogType type,
                        const char* file,
                        const std::string& name);
  ~ScopedDeviceLogIfSlow();

 private:
  const char* file_;
  LogType type_;
  std::string name_;
  base::ElapsedTimer timer_;
};

}  // namespace internal

}  // namespace device_event_log

#endif  // DEVICE_EVENT_LOG_DEVICE_EVENT_LOG_H_
