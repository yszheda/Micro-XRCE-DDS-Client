// Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef UXR_CLIENT_CORE_SESSION_STREAM_INPUT_RELIABLE_STREAM_H_
#define UXR_CLIENT_CORE_SESSION_STREAM_INPUT_RELIABLE_STREAM_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <uxr/client/core/session/stream/seq_num.h>

#include <stdbool.h>
#include <stddef.h>

struct ucdrStream;

typedef enum FragmentationInfo
{
    NO_FRAGMENTED,
    INTERMEDIATE_FRAGMENT,
    LAST_FRAGMENT

} FragmentationInfo;

typedef FragmentationInfo (*OnGetFragmentationInfo)(uint8_t* buffer);

typedef struct uxrInputReliableStream
{
    uint8_t* buffer;
    size_t max_message_size;
    size_t max_fragment_size;
    uint16_t history;

    size_t offset;
    size_t iterator;

    uxrSeqNum last_handled;
    uxrSeqNum last_announced;

} uxrInputReliableStream;

#ifdef __cplusplus
}
#endif

#endif // UXR_CLIENT_CORE_SESSION_STREAM_INPUT_RELIABLE_STREAM_H_
