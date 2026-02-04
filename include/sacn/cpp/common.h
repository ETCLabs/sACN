/******************************************************************************
 * Copyright 2022 ETC Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of sACN. For more information, go to:
 * https://github.com/ETCLabs/sACN
 *****************************************************************************/

#ifndef SACN_CPP_COMMON_H_
#define SACN_CPP_COMMON_H_

/**
 * @file sacn/cpp/common.h
 * @brief C++ wrapper for the sACN init/deinit functions
 */

#include "etcpal/cpp/error.h"
#include "etcpal/cpp/log.h"
#include "etcpal/cpp/uuid.h"
#include "sacn/common.h"

/**
 * @defgroup sacn_cpp_api sACN C++ Language APIs
 * @brief Native C++ APIs for interfacing with the sACN library.
 *
 * These wrap the corresponding C modules in a nicer syntax for C++ application developers.
 */

/**
 * @defgroup sacn_cpp_common Common Definitions
 * @ingroup sacn_cpp_api
 * @brief Definitions shared by other APIs in this module.
 */

/**
 * @brief A namespace which contains all C++ language definitions in the sACN library.
 */
namespace sacn
{
/** Determines whether multicast traffic is allowed through all interfaces or none. */
enum class McastMode
{
  kEnabledOnAllInterfaces,
  kDisabledOnAllInterfaces
};

/** A source discovered on an sACN network that has a CID - used by Receiver and Merge Receiver. */
using RemoteSourceHandle = sacn_remote_source_t;
/** An invalid RemoteSourceHandle value. */
constexpr RemoteSourceHandle kInvalidRemoteSourceHandle = SACN_REMOTE_SOURCE_INVALID;

/**
 * @ingroup sacn_cpp_common
 * @brief A base class for a class that receives common notification callbacks from sACN.
 */
class NotifyHandler
{
public:
  NotifyHandler()          = default;
  virtual ~NotifyHandler() = default;

  /**
   * @brief Defaulted copy constructor.
   * @param other Copy source.
   */
  NotifyHandler(const NotifyHandler& other) = default;

  /**
   * @brief Defaulted copy assignment operator.
   * @param other Copy source.
   * @return Reference to this.
   */
  NotifyHandler& operator=(const NotifyHandler& other) = default;

  /**
   * @brief Defaulted move constructor.
   * @param other Move source.
   */
  NotifyHandler(NotifyHandler&& other) = default;

  /**
   * @brief Defaulted move assignment operator.
   * @param other Move source.
   * @return Reference to this.
   */
  NotifyHandler& operator=(NotifyHandler&& other) = default;

  /**
   * @brief Notify that an error just occurred sending unicast on a socket.
   * @param err_info Information about the unicast send error that occurred.
   */
  virtual void HandleUnicastSendError(const SacnSocketErrorInfo& err_info) { ETCPAL_UNUSED_ARG(err_info); }

  /**
   * @brief Notify that an error just occurred sending multicast on a socket.
   * @param err_info Information about the multicast send error that occurred.
   */
  virtual void HandleMulticastSendError(const SacnSocketErrorInfo& err_info) { ETCPAL_UNUSED_ARG(err_info); }
};

/**
 * @ingroup sacn_cpp_common
 * @brief Settings for the sACN library beyond the sacn::Init overrides.
 */
struct LibrarySettings
{
  /** Callback handler for common notifications from the library. */
  NotifyHandler* notify_handler{nullptr};

  /** Configuration for send sockets (e.g. configurable sockopts). */
  SacnSendSocketConfig send_socket_config{0, 0};

  /** Sets default settings. */
  LibrarySettings() = default;
};

/**
 * @cond device_c_callbacks
 * Callbacks from underlying device library to be forwarded
 */
namespace internal
{
extern "C" inline void CommonCbUnicastSendError(const SacnSocketErrorInfo* err_info, void* context)
{
  if (err_info && context)
    static_cast<NotifyHandler*>(context)->HandleUnicastSendError(*err_info);
}

extern "C" inline void CommonCbMulticastSendError(const SacnSocketErrorInfo* err_info, void* context)
{
  if (err_info && context)
    static_cast<NotifyHandler*>(context)->HandleMulticastSendError(*err_info);
}

inline SacnCommonCallbacks GetCommonCallbacks(NotifyHandler* notify_handler)
{
  SacnCommonCallbacks callbacks = {internal::CommonCbUnicastSendError, internal::CommonCbMulticastSendError,
                                   notify_handler};
  return callbacks;
}

};  // namespace internal

/**
 * @endcond
 */

/**
 * @ingroup sacn_cpp_common
 * @brief Initialize the sACN library.
 *
 * Wraps sacn_init(). Does all initialization required before the sACN API modules can be
 * used.
 *
 * This is an overload of Init that doesn't specify a logger and assumes all network interfaces should be used.
 *
 * @param settings Optional library settings.
 * @return etcpal::Error::Ok(): Initialization successful.
 * @return Errors from sacn_init().
 */
inline etcpal::Error Init(const LibrarySettings& settings = {})
{
  SacnNetintConfig netint_config   = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.send_socket_config = settings.send_socket_config;

  auto callbacks = internal::GetCommonCallbacks(settings.notify_handler);
  return sacn_init_with_cb(nullptr, &netint_config, settings.notify_handler ? &callbacks : nullptr);
}

/**
 * @ingroup sacn_cpp_common
 * @brief Initialize the sACN library.
 *
 * Wraps sacn_init(). Does all initialization required before the sACN API modules can be used.
 *
 * This is an overload of Init that only takes a logger. It assumes all network interfaces should be used.
 *
 * @param log_params (optional) Log parameters for the sACN library to use to log messages. If
 *                   not provided, no logging will be performed.
 * @param settings Optional library settings.
 * @return etcpal::Error::Ok(): Initialization successful.
 * @return Errors from sacn_init().
 */
inline etcpal::Error Init(const EtcPalLogParams* log_params,
                          const LibrarySettings& settings = {})
{
  SacnNetintConfig netint_config   = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.send_socket_config = settings.send_socket_config;

  auto callbacks = internal::GetCommonCallbacks(settings.notify_handler);
  return sacn_init_with_cb(log_params, &netint_config, settings.notify_handler ? &callbacks : nullptr);
}

/**
 * @ingroup sacn_cpp_common
 * @brief Initialize the sACN library.
 *
 * Wraps sacn_init(). Does all initialization required before the sACN API modules can be used.
 *
 * This is an overload of Init that defaults to using all system interfaces for multicast traffic, but can also be used
 * to disable multicast traffic on all interfaces.
 *
 * @param log_params (optional) Log parameters for the sACN library to use to log messages. If
 *                   not provided, no logging will be performed.
 * @param mcast_mode This controls whether or not multicast traffic is allowed on the system's network interfaces.
 * @param settings Optional library settings.
 * @return etcpal::Error::Ok(): Initialization successful.
 * @return Errors from sacn_init().
 */
inline etcpal::Error Init(const EtcPalLogParams* log_params,
                          McastMode              mcast_mode,
                          const LibrarySettings& settings = {})
{
  SacnNetintConfig netint_config   = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.send_socket_config = settings.send_socket_config;

  if (mcast_mode == McastMode::kDisabledOnAllInterfaces)
    netint_config.no_netints = true;

  auto callbacks = internal::GetCommonCallbacks(settings.notify_handler);
  return sacn_init_with_cb(log_params, &netint_config, settings.notify_handler ? &callbacks : nullptr);
}

/**
 * @ingroup sacn_cpp_common
 * @brief Initialize the sACN library.
 *
 * Wraps sacn_init(). Does all initialization required before the sACN API modules can be
 * used.
 *
 * @param log_params Log parameters for the sACN library to use to log messages. If not provided, no logging will be
 *                   performed.
 * @param sys_netints If !empty, this is the list of system interfaces the library will be limited to, and the status
 *                    codes are filled in.  If empty, the library is allowed to use all available system interfaces.
 * @param settings Optional library settings.
 * @return etcpal::Error::Ok(): Initialization successful.
 * @return Errors from sacn_init().
 */
inline etcpal::Error Init(const EtcPalLogParams*           log_params,
                          std::vector<SacnMcastInterface>& sys_netints,
                          const LibrarySettings&           settings = {})
{
  SacnNetintConfig netint_config   = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.netints            = sys_netints.data();
  netint_config.num_netints        = sys_netints.size();
  netint_config.send_socket_config = settings.send_socket_config;

  auto callbacks = internal::GetCommonCallbacks(settings.notify_handler);
  return sacn_init_with_cb(log_params, &netint_config, settings.notify_handler ? &callbacks : nullptr);
}

/**
 * @ingroup sacn_cpp_common
 * @brief Initialize the sACN library.
 *
 * Wraps sacn_init(). Does all initialization required before the sACN API modules can be
 * used.
 *
 * This is an overload of Init that does not enable logging.
 *
 * @param sys_netints If !empty, this is the list of system interfaces the library will be limited to, and the status
 *                    codes are filled in.  If empty, the library is allowed to use all available system interfaces.
 * @param settings Optional library settings.
 * @return etcpal::Error::Ok(): Initialization successful.
 * @return Errors from sacn_init().
 */
inline etcpal::Error Init(std::vector<SacnMcastInterface>& sys_netints,
                          const LibrarySettings&           settings = {})
{
  SacnNetintConfig netint_config   = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.netints            = sys_netints.data();
  netint_config.num_netints        = sys_netints.size();
  netint_config.send_socket_config = settings.send_socket_config;

  auto callbacks = internal::GetCommonCallbacks(settings.notify_handler);
  return sacn_init_with_cb(nullptr, &netint_config, settings.notify_handler ? &callbacks : nullptr);
}

/**
 * @ingroup sacn_cpp_common
 * @brief Initialize the sACN library.
 *
 * Wraps sacn_init(). Does all initialization required before the sACN API modules can be
 * used.
 *
 * This is an overload of Init that defaults to using all system interfaces for multicast traffic, but can also be used
 * to disable multicast traffic on all interfaces.
 *
 * @param logger Logger instance for the sACN library to use to log messages.
 * @param mcast_mode This controls whether or not multicast traffic is allowed on the system's network interfaces.
 * @param settings Optional library settings.
 * @return etcpal::Error::Ok(): Initialization successful.
 * @return Errors from sacn_init().
 */
inline etcpal::Error Init(const etcpal::Logger&  logger,
                          McastMode              mcast_mode = McastMode::kEnabledOnAllInterfaces,
                          const LibrarySettings& settings   = {})
{
  SacnNetintConfig netint_config   = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.send_socket_config = settings.send_socket_config;

  if (mcast_mode == McastMode::kDisabledOnAllInterfaces)
    netint_config.no_netints = true;

  auto callbacks = internal::GetCommonCallbacks(settings.notify_handler);
  return sacn_init_with_cb(&logger.log_params(), &netint_config, settings.notify_handler ? &callbacks : nullptr);
}

/**
 * @ingroup sacn_cpp_common
 * @brief Initialize the sACN library.
 *
 * Wraps sacn_init(). Does all initialization required before the sACN API modules can be
 * used.
 *
 * @param logger Logger instance for the sACN library to use to log messages.
 * @param settings Optional library settings.
 * @return etcpal::Error::Ok(): Initialization successful.
 * @return Errors from sacn_init().
 */
inline etcpal::Error Init(const etcpal::Logger& logger, const LibrarySettings& settings = {})
{
  SacnNetintConfig netint_config   = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.send_socket_config = settings.send_socket_config;

  auto callbacks = internal::GetCommonCallbacks(settings.notify_handler);
  return sacn_init_with_cb(&logger.log_params(), &netint_config, settings.notify_handler ? &callbacks : nullptr);
}

/**
 * @ingroup sacn_cpp_common
 * @brief Initialize the sACN library.
 *
 * Wraps sacn_init(). Does all initialization required before the sACN API modules can be used.
 *
 * @param logger Logger instance for the sACN library to use to log messages.
 * @param sys_netints If !empty, this is the list of system interfaces the library will be limited to, and the status
 *                    codes are filled in.  If empty, the library is allowed to use all available system interfaces.
 * @param settings Optional library settings.
 * @return etcpal::Error::Ok(): Initialization successful.
 * @return Errors from sacn_init().
 */
inline etcpal::Error Init(const etcpal::Logger&            logger,
                          std::vector<SacnMcastInterface>& sys_netints,
                          const LibrarySettings&           settings = {})
{
  SacnNetintConfig netint_config   = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.netints            = sys_netints.data();
  netint_config.num_netints        = sys_netints.size();
  netint_config.send_socket_config = settings.send_socket_config;

  auto callbacks = internal::GetCommonCallbacks(settings.notify_handler);
  return sacn_init_with_cb(&logger.log_params(), &netint_config, settings.notify_handler ? &callbacks : nullptr);
}

/**
 * @ingroup sacn_cpp_common
 * @brief Deinitialize the sACN library.
 *
 * Closes all connections, deallocates all resources and joins the background thread. No sACN
 * API functions are usable after this function is called.
 *
 * This function is not thread safe with respect to other sACN API functions. Make sure to join your threads that use
 * the APIs before calling this.
 */
inline void Deinit()
{
  return sacn_deinit();
}

/**
 * @ingroup sacn_cpp_common
 * @brief Converts a remote source CID to the corresponding handle, or #SACN_REMOTE_SOURCE_INVALID if not found.
 *
 * This is a simple conversion from a remote source CID to it's corresponding remote source handle. A handle will be
 * returned only if it is a source that has been discovered by a receiver, merge receiver, or source detector.
 *
 * @param[in] source_cid The UUID of the remote source CID.
 * @return The remote source handle, or #SACN_REMOTE_SOURCE_INVALID if not found.
 */
inline RemoteSourceHandle GetRemoteSourceHandle(const etcpal::Uuid& source_cid)
{
  return sacn_get_remote_source_handle(&source_cid.get());
}

/**
 * @ingroup sacn_cpp_common
 * @brief Converts a remote source handle to the corresponding source CID.
 *
 * @param[in] source_handle The handle of the remote source.
 *
 * @return The UUID of the source CID if there was no error.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotFound: The source handle does not match a source that was found by a receiver, merge receiver,
 * or source detector.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Expected<etcpal::Uuid> GetRemoteSourceCid(RemoteSourceHandle source_handle)
{
  EtcPalUuid cid;
  etcpal_error_t error = sacn_get_remote_source_cid(source_handle, &cid);
  etcpal::Uuid cid_to_return = cid;

  if (error == kEtcPalErrOk)
    return cid_to_return;

  return error;
}

};  // namespace sacn

#endif  // SACN_CPP_COMMON_H_
