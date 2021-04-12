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

#include "sacn/private/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/pdu.h"

#include "sacn/private/source_detector_state.h"

/****************************** Private macros *******************************/

/***************************** Private constants *****************************/

/****************************** Private types ********************************/

/**************************** Private variables ******************************/

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
void process_universe_discovery_page(SacnSourceDetector* source_detector, const SacnUniverseDiscoveryPage* page)
{
  SourceDetectorSourceUpdatedNotification source_updated = SRC_DETECTOR_SOURCE_UPDATED_DEFAULT_INIT;

  if (sacn_lock())
  {
    SacnUniverseDiscoverySource* source = NULL;
    if (lookup_universe_discovery_source(page->sender_cid, &source) != kEtcPalErrOk)
      add_sacn_universe_discovery_source(page->sender_cid, &source);  // TODO: Handle source limit exceeded

    if (source)
    {
      etcpal_timer_reset(&source->expiration_timer);

      // The pages are tracked here to make sure that source_updated only notifies when the universe list is a complete
      // set of consecutive pages, from 0 to the last page. It's assumed the pages have been sent in order.
      if ((page->page == 0) || (page->page == source->next_page))
      {
        if (page->page == 0)
        {
          source->next_universe_index = 0;
          source->next_page = 0;
        }

        // If this page modifies the universe list:
        if ((page->num_universes > (source->num_universes - source->next_universe_index)) ||
            (memcmp(&source->universes[source->next_universe_index], page->universes,
                    page->num_universes * sizeof(uint16_t)) != 0))
        {
          source->universes_dirty = true;

          // Remove the remainder of the current universe list and then append.
          replace_universe_discovery_universes(source, source->next_universe_index, page->universes,
                                               page->num_universes);
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

          if (source->universes_dirty)
          {
            source->universes_dirty = false;

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
    source_updated.callback(source_updated.cid, source_updated.name,
                            source_updated.num_sourced_universes ? source_updated.sourced_universes : NULL,
                            source_updated.num_sourced_universes, source_updated.context);
  }

  CLEAR_BUF(&source_updated, sourced_universes);
}
