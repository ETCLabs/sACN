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

#include "sacn/private/mem/receiver/remote_source.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/handle_manager.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/util.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_RECEIVER_ENABLED

/**************************** Private constants ******************************/

#define SACN_REMOTE_SOURCES_MAX_RB_NODES ((SACN_RECEIVER_TOTAL_MAX_SOURCES + SACN_SOURCE_DETECTOR_MAX_SOURCES) * 2)

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

/* Macros for dynamic allocation. */
#define ALLOC_REMOTE_SOURCE_HANDLE() malloc(sizeof(SacnRemoteSourceHandle))
#define ALLOC_REMOTE_SOURCE_CID() malloc(sizeof(SacnRemoteSourceCid))
#define FREE_REMOTE_SOURCE_HANDLE(ptr) free(ptr)
#define FREE_REMOTE_SOURCE_CID(ptr) free(ptr)

#else  // SACN_DYNAMIC_MEM

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_REMOTE_SOURCE_HANDLE() etcpal_mempool_alloc(sacn_pool_recv_remote_source_handles)
#define ALLOC_REMOTE_SOURCE_CID() etcpal_mempool_alloc(sacn_pool_recv_remote_source_cids)
#define FREE_REMOTE_SOURCE_HANDLE(ptr) etcpal_mempool_free(sacn_pool_recv_remote_source_handles, ptr)
#define FREE_REMOTE_SOURCE_CID(ptr) etcpal_mempool_free(sacn_pool_recv_remote_source_cids, ptr)

#endif  // SACN_DYNAMIC_MEM

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_recv_remote_source_handles, SacnRemoteSourceHandle,
                      SACN_RECEIVER_TOTAL_MAX_SOURCES + SACN_SOURCE_DETECTOR_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_recv_remote_source_cids, SacnRemoteSourceCid,
                      SACN_RECEIVER_TOTAL_MAX_SOURCES + SACN_SOURCE_DETECTOR_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_recv_remote_source_rb_nodes, EtcPalRbNode, SACN_REMOTE_SOURCES_MAX_RB_NODES);
#endif  // !SACN_DYNAMIC_MEM

static EtcPalRbTree remote_source_handles;
static EtcPalRbTree remote_source_cids;

static IntHandleManager remote_source_handle_manager;

/*********************** Private function prototypes *************************/

static int uuid_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static bool remote_source_handle_in_use(int handle_val, void* cookie);

static void remote_source_handle_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);
static void remote_source_cid_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);
static EtcPalRbNode* remote_source_node_alloc(void);
static void remote_source_node_dealloc(EtcPalRbNode* node);

/*************************** Function definitions ****************************/

etcpal_error_t init_remote_sources(void)
{
  etcpal_error_t res = kEtcPalErrOk;

  const int kMaxValidHandleValue = 0xffff - 1;  // This is the max VALID value, therefore invalid - 1 (0xffff - 1)
  init_int_handle_manager(&remote_source_handle_manager, kMaxValidHandleValue, remote_source_handle_in_use, NULL);

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacn_pool_recv_remote_source_handles);
  res |= etcpal_mempool_init(sacn_pool_recv_remote_source_cids);
  res |= etcpal_mempool_init(sacn_pool_recv_remote_source_rb_nodes);
#endif  // !SACN_DYNAMIC_MEM

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&remote_source_handles, uuid_compare, remote_source_node_alloc, remote_source_node_dealloc);
    etcpal_rbtree_init(&remote_source_cids, remote_source_compare, remote_source_node_alloc,
                       remote_source_node_dealloc);
  }

  return res;
}

void deinit_remote_sources(void)
{
  etcpal_rbtree_clear_with_cb(&remote_source_handles, remote_source_handle_tree_dealloc);
  etcpal_rbtree_clear_with_cb(&remote_source_cids, remote_source_cid_tree_dealloc);
}

etcpal_error_t add_remote_source_handle(const EtcPalUuid* cid, sacn_remote_source_t* handle)
{
  if (!SACN_ASSERT_VERIFY(cid) || !SACN_ASSERT_VERIFY(handle))
    return kEtcPalErrSys;

  etcpal_error_t result = kEtcPalErrOk;

  SacnRemoteSourceHandle* existing_handle = (SacnRemoteSourceHandle*)etcpal_rbtree_find(&remote_source_handles, cid);

  if (existing_handle)
  {
    SacnRemoteSourceCid* existing_cid =
        (SacnRemoteSourceCid*)etcpal_rbtree_find(&remote_source_cids, &existing_handle->handle);
    if (SACN_ASSERT_VERIFY(existing_cid))
      ++existing_cid->refcount;
    else
      result = kEtcPalErrSys;

    *handle = existing_handle->handle;
  }
  else
  {
    SacnRemoteSourceHandle* new_handle = ALLOC_REMOTE_SOURCE_HANDLE();
    SacnRemoteSourceCid* new_cid = ALLOC_REMOTE_SOURCE_CID();

    if (new_handle && new_cid)
    {
      new_handle->cid = *cid;
      new_handle->handle = (sacn_remote_source_t)get_next_int_handle(&remote_source_handle_manager);
      new_cid->handle = new_handle->handle;
      new_cid->cid = new_handle->cid;
      new_cid->refcount = 1;

      result = etcpal_rbtree_insert(&remote_source_handles, new_handle);

      if (result == kEtcPalErrOk)
        result = etcpal_rbtree_insert(&remote_source_cids, new_cid);

      if (result == kEtcPalErrOk)
        *handle = new_handle->handle;
    }
    else
    {
      result = kEtcPalErrNoMem;
    }

    if (result != kEtcPalErrOk)
    {
      if (new_handle)
      {
        etcpal_rbtree_remove(&remote_source_handles, new_handle);
        FREE_REMOTE_SOURCE_HANDLE(new_handle);
      }

      if (new_cid)
        FREE_REMOTE_SOURCE_CID(new_cid);
    }
  }

  return result;
}

sacn_remote_source_t get_remote_source_handle(const EtcPalUuid* source_cid)
{
  if (!SACN_ASSERT_VERIFY(source_cid))
    return SACN_REMOTE_SOURCE_INVALID;

  sacn_remote_source_t result = SACN_REMOTE_SOURCE_INVALID;

  SacnRemoteSourceHandle* tree_result = (SacnRemoteSourceHandle*)etcpal_rbtree_find(&remote_source_handles, source_cid);

  if (tree_result)
    result = tree_result->handle;

  return result;
}

const EtcPalUuid* get_remote_source_cid(sacn_remote_source_t handle)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_REMOTE_SOURCE_INVALID))
    return NULL;

  const EtcPalUuid* result = NULL;

  SacnRemoteSourceCid* tree_result = (SacnRemoteSourceCid*)etcpal_rbtree_find(&remote_source_cids, &handle);

  if (tree_result)
    result = &tree_result->cid;

  return result;
}

etcpal_error_t remove_remote_source_handle(sacn_remote_source_t handle)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_REMOTE_SOURCE_INVALID))
    return kEtcPalErrSys;

  etcpal_error_t handle_result = kEtcPalErrOk;
  etcpal_error_t cid_result = kEtcPalErrOk;

  SacnRemoteSourceCid* existing_cid = (SacnRemoteSourceCid*)etcpal_rbtree_find(&remote_source_cids, &handle);

  if (existing_cid)
  {
    if (existing_cid->refcount <= 1)
    {
      handle_result =
          etcpal_rbtree_remove_with_cb(&remote_source_handles, &existing_cid->cid, remote_source_handle_tree_dealloc);
      cid_result = etcpal_rbtree_remove_with_cb(&remote_source_cids, &handle, remote_source_cid_tree_dealloc);
    }
    else
    {
      --existing_cid->refcount;
    }
  }
  else
  {
    cid_result = kEtcPalErrNotFound;
  }

  if (handle_result != kEtcPalErrOk)
    return handle_result;

  return cid_result;
}

int remote_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  if (!SACN_ASSERT_VERIFY(value_a) || !SACN_ASSERT_VERIFY(value_b))
    return 0;

  sacn_remote_source_t* a = (sacn_remote_source_t*)value_a;
  sacn_remote_source_t* b = (sacn_remote_source_t*)value_b;
  return (*a > *b) - (*a < *b);
}

int uuid_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  if (!SACN_ASSERT_VERIFY(value_a) || !SACN_ASSERT_VERIFY(value_b))
    return 0;

  const EtcPalUuid* a = (const EtcPalUuid*)value_a;
  const EtcPalUuid* b = (const EtcPalUuid*)value_b;
  return ETCPAL_UUID_CMP(a, b);
}

bool remote_source_handle_in_use(int handle_val, void* cookie)
{
  ETCPAL_UNUSED_ARG(cookie);
  return (get_remote_source_cid((sacn_remote_source_t)handle_val) != NULL);
}

/* Helper function for clearing an EtcPalRbTree containing remote source handles. */
void remote_source_handle_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  if (!SACN_ASSERT_VERIFY(node))
    return;

  FREE_REMOTE_SOURCE_HANDLE(node->value);
  remote_source_node_dealloc(node);
}

/* Helper function for clearing an EtcPalRbTree containing remote source CIDs. */
void remote_source_cid_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  if (!SACN_ASSERT_VERIFY(node))
    return;

  FREE_REMOTE_SOURCE_CID(node->value);
  remote_source_node_dealloc(node);
}

EtcPalRbNode* remote_source_node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacn_pool_recv_remote_source_rb_nodes);
#endif
}

void remote_source_node_dealloc(EtcPalRbNode* node)
{
  if (!SACN_ASSERT_VERIFY(node))
    return;

#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_recv_remote_source_rb_nodes, node);
#endif
}

#endif  // SACN_RECEIVER_ENABLED
