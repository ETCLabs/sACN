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

#include "sacn/private/mem/receiver/sampling_period_netint.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/mem/common.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_RECEIVER_ENABLED

/**************************** Private constants ******************************/

#define SACN_MAX_SAMPLING_PERIOD_NETINTS (SACN_RECEIVER_MAX_UNIVERSES * SACN_MAX_NETINTS)

#define SACN_MAX_SAMPLING_PERIOD_NETINT_RB_NODES (SACN_RECEIVER_MAX_UNIVERSES * SACN_MAX_NETINTS)

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

/* Macros for dynamic allocation. */
#define ALLOC_SAMPLING_PERIOD_NETINT() malloc(sizeof(SacnSamplingPeriodNetint))
#define FREE_SAMPLING_PERIOD_NETINT(ptr) free(ptr)

#else  // SACN_DYNAMIC_MEM

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_SAMPLING_PERIOD_NETINT() etcpal_mempool_alloc(sacn_pool_recv_sampling_period_netints)
#define FREE_SAMPLING_PERIOD_NETINT(ptr) etcpal_mempool_free(sacn_pool_recv_sampling_period_netints, ptr)

#endif  // SACN_DYNAMIC_MEM

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_recv_sampling_period_netints, SacnSamplingPeriodNetint,
                      SACN_MAX_SAMPLING_PERIOD_NETINTS);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_recv_sampling_period_netint_rb_nodes, EtcPalRbNode,
                      SACN_MAX_SAMPLING_PERIOD_NETINT_RB_NODES);
#endif  // !SACN_DYNAMIC_MEM

/*************************** Function definitions ****************************/

etcpal_error_t init_sampling_period_netints(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacn_pool_recv_sampling_period_netints);
  res |= etcpal_mempool_init(sacn_pool_recv_sampling_period_netint_rb_nodes);
#endif  // !SACN_DYNAMIC_MEM

  return res;
}

etcpal_error_t add_sacn_sampling_period_netint(EtcPalRbTree* tree, const EtcPalMcastNetintId* id,
                                               bool in_future_sampling_period)
{
  if (!SACN_ASSERT_VERIFY(tree) || !SACN_ASSERT_VERIFY(id))
    return kEtcPalErrSys;

  etcpal_error_t result = kEtcPalErrOk;

  SacnSamplingPeriodNetint* netint = ALLOC_SAMPLING_PERIOD_NETINT();
  if (!netint)
    result = kEtcPalErrNoMem;

  if (result == kEtcPalErrOk)
  {
    netint->id = *id;
    netint->in_future_sampling_period = in_future_sampling_period;

    result = etcpal_rbtree_insert(tree, netint);
  }

  if (result != kEtcPalErrOk)
  {
    if (netint)
      FREE_SAMPLING_PERIOD_NETINT(netint);
  }

  if (result == kEtcPalErrExists)
    result = kEtcPalErrOk;  // Application may feed in duplicates - this function should treat this as ok.

  return result;
}

void remove_current_sampling_period_netints(EtcPalRbTree* tree)
{
  if (!SACN_ASSERT_VERIFY(tree))
    return;

  SacnSamplingPeriodNetint* next_current_sp_netint = NULL;
  do
  {
    next_current_sp_netint = NULL;

    EtcPalRbIter iter;
    etcpal_rbiter_init(&iter);
    for (SacnSamplingPeriodNetint* sp_netint = etcpal_rbiter_first(&iter, tree); sp_netint;
         sp_netint = etcpal_rbiter_next(&iter))
    {
      if (!sp_netint->in_future_sampling_period)
      {
        next_current_sp_netint = sp_netint;
        break;
      }
    }

    if (next_current_sp_netint)
    {
      etcpal_error_t remove_res =
          etcpal_rbtree_remove_with_cb(tree, next_current_sp_netint, sampling_period_netint_tree_dealloc);
      SACN_ASSERT_VERIFY(remove_res == kEtcPalErrOk);
      (void)remove_res;  // Fix for warning C4189
    }
  } while (next_current_sp_netint);
}

etcpal_error_t remove_sampling_period_netint(EtcPalRbTree* tree, const EtcPalMcastNetintId* id)
{
  if (!SACN_ASSERT_VERIFY(tree) || !SACN_ASSERT_VERIFY(id))
    return kEtcPalErrSys;

  return etcpal_rbtree_remove_with_cb(tree, id, sampling_period_netint_tree_dealloc);
}

int sampling_period_netint_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  if (!SACN_ASSERT_VERIFY(value_a) || !SACN_ASSERT_VERIFY(value_b))
    return 0;

  EtcPalMcastNetintId* a = (EtcPalMcastNetintId*)value_a;
  EtcPalMcastNetintId* b = (EtcPalMcastNetintId*)value_b;

  int res = ((int)a->ip_type > (int)b->ip_type) - ((int)a->ip_type < (int)b->ip_type);
  if (res == 0)
    res = (a->index > b->index) - (a->index < b->index);

  return res;
}

/* Helper function for clearing an EtcPalRbTree containing sampling period netints. */
void sampling_period_netint_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  if (!SACN_ASSERT_VERIFY(node))
    return;

  FREE_SAMPLING_PERIOD_NETINT(node->value);
  sampling_period_netint_node_dealloc(node);
}

void sampling_period_netint_node_dealloc(EtcPalRbNode* node)
{
  if (!SACN_ASSERT_VERIFY(node))
    return;

#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_recv_sampling_period_netint_rb_nodes, node);
#endif
}

EtcPalRbNode* sampling_period_netint_node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacn_pool_recv_sampling_period_netint_rb_nodes);
#endif
}

#endif  // SACN_RECEIVER_ENABLED
