/******************************************************************************
 * Copyright 2021 ETC Inc.
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

/**
 * @file sacn/private/opts.h
 * @brief sACN configuration options.
 *
 * Default values for all of sACN's @ref sacnopts "compile-time configuration options".
 */

#ifndef SACN_PRIVATE_OPTS_H_
#define SACN_PRIVATE_OPTS_H_

/**
 * @defgroup sacnopts sACN Configuration Options
 * @ingroup sACN
 * @brief Compile-time configuration options for sACN.
 *
 * Default values are indicated as the value of the @#define. Default values can be overriden by
 * defining the option in your project's `sacn_config.h` file. See @ref building_and_integrating
 * for more information on the `sacn_config.h` file.
 */

#if SACN_HAVE_CONFIG_H
/* User configuration options. Any non-defined ones will get their defaults in this file. */
#include "sacn_config.h"
#endif

#include "etcpal/thread.h"

/* Some option hints based on well-known compile definitions */

/** \cond */

/* Are we being compiled for a full-featured OS? */
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION)
#define SACN_FULL_OS_AVAILABLE_HINT 1
#else
#define SACN_FULL_OS_AVAILABLE_HINT 0
#endif

/** \endcond */

/*************************** sACN Global Options *****************************/

/**
 * @defgroup sacnopts_global Global Options
 * @ingroup sacnopts
 *
 * Options that apply to both @ref sacn_source and @ref sacn_receiver.
 * @{
 */

/**
 * @brief Use dynamic memory allocation.
 *
 * If defined nonzero, sACN manages memory dynamically using malloc() and free() from stdlib.h.
 * Otherwise, sACN uses fixed-size pools through @ref etcpal_mempool. The size of the pools is
 * controlled with other config options.
 */
#ifndef SACN_DYNAMIC_MEM
#define SACN_DYNAMIC_MEM SACN_FULL_OS_AVAILABLE_HINT
#endif

/**
 * @brief Enable message logging from the sACN library.
 *
 * If defined nonzero, the log function pointer parameter taken by sACN init functions is used to
 * log messages from the library.
 */
#ifndef SACN_LOGGING_ENABLED
#define SACN_LOGGING_ENABLED 1
#endif

/**
 * @brief A string which will be prepended to all log messages from the sACN library.
 */
#ifndef SACN_LOG_MSG_PREFIX
#define SACN_LOG_MSG_PREFIX "sACN: "
#endif

/**
 * @brief The debug assert used by the sACN library.
 *
 * By default, just uses the C library assert. If redefining this, it must be redefined as a macro
 * taking a single argument (the assertion expression).
 */
#ifndef SACN_ASSERT
#include <assert.h>
#define SACN_ASSERT(expr) assert(expr)
#endif

/**
 * @brief Enable ETC's per-address priority extension to sACN.
 *
 * If defined nonzero, the logic of @ref sacn_receiver "sACN Receiver" changes to handle ETC's
 * per-address priority sACN extension. An additional callback function is also enabled to be
 * notified that a source has stopped sending per-address priority.
 */
#ifndef SACN_ETC_PRIORITY_EXTENSION
#define SACN_ETC_PRIORITY_EXTENSION 1
#endif

/**
 * @brief Allow loopback of sACN to the local host (by setting the relevant socket option).
 *
 * Most, but not all, platforms have this option enabled by default. This is necessary if a host
 * wants to receive the same sACN it is sending.
 */
#ifndef SACN_LOOPBACK
#define SACN_LOOPBACK 1
#endif

/**
 * @brief The maximum number of network interfaces that can used by the sACN library.
 *
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0.
 */
#ifndef SACN_MAX_NETINTS
#define SACN_MAX_NETINTS 2
#endif

/**
 * @}
 */

/*************************** sACN Receive Options ****************************/

/**
 * @defgroup sacnopts_receiver sACN Receiver Options
 * @ingroup sacnopts
 *
 * Configuration options for the @ref sacn_receiver module.
 * @{
 */

/**
 * @brief The priority of each sACN receiver thread.
 *
 * This is usually only meaningful on real-time systems.
 */
#ifndef SACN_RECEIVER_THREAD_PRIORITY
#define SACN_RECEIVER_THREAD_PRIORITY ETCPAL_THREAD_DEFAULT_PRIORITY
#endif

/**
 * @brief The stack size of each sACN receiver thread.
 *
 * It's usually only necessary to worry about this on real-time or embedded systems.
 */
#ifndef SACN_RECEIVER_THREAD_STACK
#define SACN_RECEIVER_THREAD_STACK ETCPAL_THREAD_DEFAULT_STACK
#endif

/* Infinite read blocks are not supported due to the potential for hangs on shutdown. */
#if defined(SACN_RECEIVER_READ_TIMEOUT_MS) && SACN_RECEIVER_READ_TIMEOUT_MS < 0
#undef SACN_RECEIVER_READ_TIMEOUT_MS /* It will get the default value below */
#endif

/**
 * @brief The maximum amount of time that a call to sacnrecv_read() will block waiting for data, in
 *        milliseconds.
 *
 * It is recommended to keep this time short to avoid delays on shutdown.
 */
#ifndef SACN_RECEIVER_READ_TIMEOUT_MS
#define SACN_RECEIVER_READ_TIMEOUT_MS 100
#endif

/**
 * @brief The maximum number of sACN universes that can be listened to simultaneously.
 *
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0.
 */
#ifndef SACN_RECEIVER_MAX_UNIVERSES
#define SACN_RECEIVER_MAX_UNIVERSES 8
#endif

// Be sure to check SACN_RECEIVER_MAX_UNIVERSES, as it is illegal to declare a 0-size or negative-size array in C.
#if !SACN_DYNAMIC_MEM && (SACN_RECEIVER_MAX_UNIVERSES <= 0)
#error "SACN_RECEIVER_MAX_UNIVERSES is invalid! Please define it to be a positive, non-zero value."
#endif

/**
 * @brief The maximum number of sources that can be tracked on each universe.
 *
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0. This includes sources at any priority; all
 * sources for a given universe are tracked, even those with a lower priority than the
 * highest-priority source.
 */
#ifndef SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE
#define SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE 8
#endif

/**
 * @brief The total maximum number of sources that can be tracked.
 *
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0. Defaults to #SACN_RECEIVER_MAX_UNIVERSES *
 * #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, but can be made lower if an application wants to impose a
 * global hard source limit. The total number of sources that will be handled will be the lower of
 * #SACN_RECEIVER_TOTAL_MAX_SOURCES and (#SACN_RECEIVER_MAX_UNIVERSES * #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE).
 */
#ifndef SACN_RECEIVER_TOTAL_MAX_SOURCES
#define SACN_RECEIVER_TOTAL_MAX_SOURCES (SACN_RECEIVER_MAX_UNIVERSES * SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE)
#endif

/**
 * @brief If set to 1, bind only two sockets per thread to reduce duplicate multicast traffic.
 *
 * Each sACN receiver socket joins up to #SACN_RECEIVER_MAX_SUBS_PER_SOCKET unique multicast
 * groups. If #SACN_RECEIVER_LIMIT_BIND is 0, then each socket binds to the wildcard. On certain
 * platforms, this results in multicast traffic being duplicated between sockets. In this case,
 * setting #SACN_RECEIVER_LIMIT_BIND to 1 will limit sACN to binding (and polling) just two sockets
 * per thread (one for IPv4 and another for IPv6). The purpose is to cause all multicast and
 * unicast traffic to go to the bound sockets and reduce duplication. This has been verified to
 * work on Linux and lwIP.
 * 
 * Set this to 0 in your sacn_config.h if the default causes packets to be lost on your platform.
 *
 * Don't change this option unless you know what you're doing.
 */
#ifndef SACN_RECEIVER_LIMIT_BIND
#define SACN_RECEIVER_LIMIT_BIND (!_WIN32 && !__APPLE__)
#endif

/**
 * @brief The maximum number of multicast subscriptions supported per shared socket.
 *
 * We cap multicast subscriptions at a certain number to keep it below the system limit.
 *
 * Don't change this option unless you know what you're doing.
 */
#ifndef SACN_RECEIVER_MAX_SUBS_PER_SOCKET
#define SACN_RECEIVER_MAX_SUBS_PER_SOCKET 20
#endif

/** @cond */
/* TODO investigate. Windows value was 110592 */
#ifndef SACN_RECEIVER_SOCKET_RCVBUF_SIZE
#define SACN_RECEIVER_SOCKET_RCVBUF_SIZE 32768
#endif
/** @endcond */

/**
 * @brief Currently unused; will be used in the future.
 */
#undef SACN_RECEIVER_MAX_THREADS
#define SACN_RECEIVER_MAX_THREADS 1

/**
 * @}
 */

/***************************** sACN Send Options *****************************/

/**
 * @defgroup sacnopts_send sACN Send Options
 * @ingroup sacnopts
 *
 * Configuration options for the @ref sacn_source module.
 * @{
 */

/** @cond */
/* TODO investigate. Windows value was 20 */
#ifndef SACN_SOURCE_MULTICAST_TTL
#define SACN_SOURCE_MULTICAST_TTL 64
#endif
/** @endcond */

/**
 * @brief The maximum number of sources that can be created.
 *
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0.
 *
 * If this is set to 0, no memory pools are allocated, and the Source API is disabled.
 */
#ifndef SACN_SOURCE_MAX_SOURCES
#define SACN_SOURCE_MAX_SOURCES 1
#endif

/**
 * @brief The maximum number of universes that a source can send to simultaneously.
 *
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0.
 *
 * If this is set to 0, no memory pools are allocated, and the Source API is disabled.
 */
#ifndef SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE
#define SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE 4
#endif

/**
 * @brief The maximum number of unicast destinations per universe that a source can send to simultaneously.
 *
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0.
 */
#ifndef SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE
#define SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE 4
#endif

/**
 * @}
 */

/***************************** sACN DMX Merger Options *****************************/

/**
 * @defgroup sacnopts_dmx_merger sACN DMX Merger Options
 * @ingroup sacnopts
 *
 * Configuration options for the @ref sacn_dmx_merger module.
 * @{
 */

/**
 * @brief The maximum number of mergers that can be instanced.
 *
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0.
 */
#ifndef SACN_DMX_MERGER_MAX_MERGERS
#define SACN_DMX_MERGER_MAX_MERGERS SACN_RECEIVER_MAX_UNIVERSES
#endif

/**
 * @brief The maximum number of sources that can be merged on each merger instance.
 *
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0.
 */
#ifndef SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER
#define SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE
#endif

/**
 * @}
 */

/***************************** sACN Source Detector Options *****************************/

/**
 * @defgroup sacnopts_source_detector sACN Source Detector Options
 * @ingroup sacnopts
 *
 * Configuration options for the @ref sacn_source_detector module.
 * @{
 */

/* TODO: These constants are here for the case we need a separate thread for source detection.
 *  It could very well be that some sACN receiver thread in the library can drive this as well,
 *  in which case these constants don't need to be here.
 */

/**
 * @brief The priority of the sACN source detector thread.
 *
 * This is usually only meaningful on real-time systems.
 */
#ifndef SACN_SOURCE_DETECTOR_THREAD_PRIORITY
#define SACN_SOURCE_DETECTOR_THREAD_PRIORITY ETCPAL_THREAD_DEFAULT_PRIORITY
#endif

/**
 * @brief The stack size of each sACN source detector thread.
 *
 * It's usually only necessary to worry about this on real-time or embedded systems.
 */
#ifndef SACN_SOURCE_DETECTOR_THREAD_STACK
#define SACN_SOURCE_DETECTOR_THREAD_STACK ETCPAL_THREAD_DEFAULT_STACK
#endif

/* Infinite read blocks are not supported due to the potential for hangs on shutdown. */
#if defined(SACN_SOURCE_DETECTOR_READ_TIMEOUT_MS) && SACN_SOURCE_DETECTOR_READ_TIMEOUT_MS < 0
#undef SACN_SOURCE_DETECTOR_READ_TIMEOUT_MS /* It will get the default value below */
#endif

//TODO change this to whatever you need, or remove the constant.
/**
 * @brief The maximum amount of time the Source Detector thread will block waiting for data, in
 *        milliseconds.
 *
 * It is recommended to keep this time short to avoid delays on shutdown.
 */
#ifndef SACN_SOURCE_DETECTOR_READ_TIMEOUT_MS
#define SACN_SOURCE_DETECTOR_READ_TIMEOUT_MS 100
#endif

/**
 * @brief The maximum number of sACN sources that can be monitored.
 * 
 * This number is intentionally set on the small side.  This module
 * is more likely to be needed by applications that use dynamic memory.
 * 
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0.
 */
#ifndef SACN_SOURCE_DETECTOR_MAX_SOURCES
#define SACN_SOURCE_DETECTOR_MAX_SOURCES 5
#endif

/**
 * @brief The maximum number of sACN universes that can be tracked on each source.
 *
 * This number is intentionally set on the small side.  This module
 * is more likely to be needed by applications that use dynamic memory.
 * 
 * Meaningful only if #SACN_DYNAMIC_MEM is defined to 0.
 */
#ifndef SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE
#define SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE 5
#endif


/**
 * @}
 */

#endif /* SACN_PRIVATE_OPTS_H_ */
