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

#include "sacn/private/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/pdu.h"

#include "sacn/private/source_detector_state.h"

#if SACN_SOURCE_DETECTOR_ENABLED

/*********************** Private function prototypes *************************/

static void process_universe_discovery_page(SacnSourceDetector* source_detector, const SacnUniverseDiscoveryPage* page);

/*************************** Function definitions ****************************/

etcpal_error_t sacn_source_detector_state_init(void)
{
  // Nothing to do

  return kEtcPalErrOk;
}

void sacn_source_detector_state_deinit(void)
{
  // Nothing to do
}

// Needs lock
size_t get_source_detector_netints(const SacnSourceDetector* detector, EtcPalMcastNetintId* netints,
                                   size_t netints_size)
{
  for (size_t i = 0; netints && (i < netints_size) && (i < detector->netints.num_netints); ++i)
    netints[i] = detector->netints.netints[i];

  return detector->netints.num_netints;
}

// Takes lock
void handle_sacn_universe_discovery_packet(SacnRecvThreadContext* context, const uint8_t* data, size_t datalen,
                                           const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr,
                                           const char* source_name)
{
  SacnSourceDetector* source_detector = NULL;

  if (sacn_lock())
  {
    source_detector = context->source_detector;
    sacn_unlock();
  }

  if (source_detector)
  {
    SacnUniverseDiscoveryPage page;
    page.sender_cid = sender_cid;
    page.from_addr = from_addr;
    page.source_name = source_name;

    const uint8_t* universe_buf;
    if (parse_sacn_universe_discovery_layer(data, datalen, &page.page, &page.last_page, &universe_buf,
                                            &page.num_universes))
    {
#if SACN_DYNAMIC_MEM
      uint16_t* universes = page.num_universes ? calloc(page.num_universes, sizeof(uint16_t)) : NULL;
#else
      uint16_t universes[SACN_MAX_UNIVERSES_PER_PAGE];
#endif
      page.universes = universes;

      parse_sacn_universe_list(universe_buf, page.num_universes, universes);
      process_universe_discovery_page(source_detector, &page);

#if SACN_DYNAMIC_MEM
      if (universes)
        free(universes);
#endif
    }
    else if (SACN_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(sender_cid, cid_str);
      SACN_LOG_WARNING("Ignoring malformed sACN universe discovery packet from component %s", cid_str);
    }
  }
}

// Takes lock
void process_source_detector(SacnRecvThreadContext* recv_thread_context)
{
  SourceDetectorSourceExpiredNotification source_expired = SRC_DETECTOR_SOURCE_EXPIRED_DEFAULT_INIT;

  if (sacn_lock())
  {
    SacnSourceDetector* source_detector = recv_thread_context->source_detector;
    if (source_detector)
    {
      EtcPalRbIter iter;
      for (SacnUniverseDiscoverySource* source = get_first_universe_discovery_source(&iter); source;
           source = get_next_universe_discovery_source(&iter))
      {
        if (etcpal_timer_is_expired(&source->expiration_timer))
        {
          source_expired.callback = source_detector->callbacks.source_expired;
          source_expired.context = source_detector->callbacks.context;
          add_sacn_source_detector_expired_source(&source_expired, source->handle, source->name);
          source_detector->suppress_source_limit_exceeded_notification = false;
        }
      }

      for (size_t i = 0; i < source_expired.num_expired_sources; ++i)
        remove_sacn_universe_discovery_source(source_expired.expired_sources[i].handle);
    }

    sacn_unlock();
  }

  if (source_expired.callback)
  {
    for (size_t i = 0; i < source_expired.num_expired_sources; ++i)
    {
      source_expired.callback(source_expired.expired_sources[i].handle, &source_expired.expired_sources[i].cid,
                              source_expired.expired_sources[i].name, source_expired.context);
    }
  }

  CLEAR_BUF(&source_expired, expired_sources);
}

// Takes lock
void process_universe_discovery_page(SacnSourceDetector* source_detector, const SacnUniverseDiscoveryPage* page)
{
  SourceDetectorSourceUpdatedNotification source_updated = SRC_DETECTOR_SOURCE_UPDATED_DEFAULT_INIT;
  SourceDetectorLimitExceededNotification limit_exceeded = SRC_DETECTOR_LIMIT_EXCEEDED_DEFAULT_INIT;

  if (sacn_lock())
  {
    SacnUniverseDiscoverySource* source = NULL;
    etcpal_error_t source_result =
        lookup_universe_discovery_source(get_remote_source_handle(page->sender_cid), &source);

    if (source_result == kEtcPalErrNotFound)
    {
#if SACN_DYNAMIC_MEM
      if ((source_detector->source_count_max != SACN_SOURCE_DETECTOR_INFINITE) &&
          (get_num_universe_discovery_sources() >= (size_t)source_detector->source_count_max))
      {
        source_result = kEtcPalErrNoMem;
      }
#endif
      if (source_result != kEtcPalErrNoMem)
        source_result = add_sacn_universe_discovery_source(page->sender_cid, page->source_name, &source);
    }

    if ((source_result == kEtcPalErrNoMem) && !source_detector->suppress_source_limit_exceeded_notification)
    {
      source_detector->suppress_source_limit_exceeded_notification = true;
      limit_exceeded.callback = source_detector->callbacks.limit_exceeded;
      limit_exceeded.context = source_detector->callbacks.context;
    }

    if (source_result == kEtcPalErrOk)
    {
      etcpal_timer_reset(&source->expiration_timer);

      // The pages are tracked here to make sure that source_updated only notifies when the universe list is a complete
      // set of consecutive pages, from 0 to the last page. It's assumed the pages have been sent in order.
      if ((page->page != 0) && (page->page != source->next_page))
      {
        // Out of sequence - start over.
        source->next_universe_index = 0;
        source->next_page = 0;
      }
      else  // This page begins or continues a sequence of consecutive pages.
      {
        if (page->page == 0)
        {
          source->next_universe_index = 0;
          source->next_page = 0;
        }

        // If this page modifies the universe list:
        size_t num_remaining_universes = (source->num_universes - source->next_universe_index);
        if ((page->num_universes > num_remaining_universes) ||
            ((page->page == page->last_page) && (page->num_universes < num_remaining_universes)) ||
            (memcmp(&source->universes[source->next_universe_index], page->universes,
                    page->num_universes * sizeof(uint16_t)) != 0))
        {
          source->universes_dirty = true;

          // Remove the remainder of the current universe list and then append.
          size_t replace_result = replace_universe_discovery_universes(
              source, source->next_universe_index, page->universes, page->num_universes,
              (size_t)source_detector->universes_per_source_max);

          // If there's not enough room for this page:
          if (replace_result < page->num_universes)
          {
            if (replace_result > 0)
            {
              // Fit as many universes as possible.
              replace_universe_discovery_universes(source, source->next_universe_index, page->universes, replace_result,
                                                   source_detector->universes_per_source_max);
            }

            if (!source->suppress_universe_limit_exceeded_notification)
            {
              source->suppress_universe_limit_exceeded_notification = true;
              limit_exceeded.callback = source_detector->callbacks.limit_exceeded;
              limit_exceeded.context = source_detector->callbacks.context;
            }
          }
        }

        if (page->page < page->last_page)
        {
          source->next_universe_index += page->num_universes;
          ++source->next_page;
        }
        else  // Last page
        {
          source->next_universe_index = 0;
          source->next_page = 0;

          // Verify the list is in ascending order if dirty. If not ascending, declare not dirty to filter.
          for (size_t i = 1; source->universes_dirty && (i < source->num_universes); ++i)
            source->universes_dirty = (source->universes[i - 1] < source->universes[i]);

          if (source->universes_dirty)
          {
            if (source->num_universes < source->last_notified_universe_count)
              source->suppress_universe_limit_exceeded_notification = false;

            source->universes_dirty = false;
            source->last_notified_universe_count = source->num_universes;

            source_updated.callback = source_detector->callbacks.source_updated;
            source_updated.cid = page->sender_cid;
            source_updated.name = page->source_name;

#if SACN_DYNAMIC_MEM
            source_updated.sourced_universes =
                source->num_universes ? calloc(source->num_universes, sizeof(uint16_t)) : NULL;
#endif
            for (size_t i = 0; i < source->num_universes; ++i)
              source_updated.sourced_universes[i] = source->universes[i];

            source_updated.num_sourced_universes = source->num_universes;
            source_updated.context = source_detector->callbacks.context;
          }
        }
      }
    }

    sacn_unlock();
  }

  if (source_updated.callback)
  {
    source_updated.callback(source_updated.handle, source_updated.cid, source_updated.name,
                            source_updated.num_sourced_universes ? source_updated.sourced_universes : NULL,
                            source_updated.num_sourced_universes, source_updated.context);
  }

  if (limit_exceeded.callback)
    limit_exceeded.callback(limit_exceeded.context);

  CLEAR_BUF(&source_updated, sourced_universes);
}

#endif  // SACN_SOURCE_DETECTOR_ENABLED
