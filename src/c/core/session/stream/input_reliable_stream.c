#include "seq_num_internal.h"
#include "input_reliable_stream_internal.h"
#include "common_reliable_stream_internal.h"
#include "../submessage_internal.h"
#include <ucdr/microcdr.h>

#include <string.h>

static bool check_last_fragment(
        uxrInputReliableStream* stream,
        uxrSeqNum* last);

static uxrSeqNum uxr_get_first_unacked(
        const uxrInputReliableStream* stream);

static FragmentationInfo uxr_get_fragmentation_info(
        uint8_t* buffer);

//==================================================================
//                             PUBLIC
//==================================================================
void uxr_init_input_reliable_stream(
        uxrInputReliableStream* stream,
        uint8_t* buffer,
        size_t max_message_size,
        size_t max_fragment_size,
        uint16_t history)
{
    // assert for history (must be 2^)
    stream->buffer = buffer;
    stream->max_message_size = max_message_size;
    stream->max_fragment_size = max_fragment_size;
    stream->history = history;

    uxr_reset_input_reliable_stream(stream);
}

void uxr_reset_input_reliable_stream(
        uxrInputReliableStream* stream)
{
    for(size_t i = 0; i < stream->history; ++i)
    {
        uint8_t* internal_buffer = uxr_get_input_buffer(stream, i);
        uxr_set_reliable_buffer_length(internal_buffer, 0);
    }

    stream->last_handled = SEQ_NUM_MAX;
    stream->last_announced = SEQ_NUM_MAX;
}

bool uxr_receive_reliable_message(
        uxrInputReliableStream* stream,
        uint16_t seq_num,
        uint8_t* buffer,
        size_t length,
        bool* message_stored)
{
    bool ready_to_read = false;

    /* Check if the seq_num is valid for the stream state */
    uxrSeqNum last_history = uxr_seq_num_add(stream->last_handled, stream->history);
    if(0 > uxr_seq_num_cmp(stream->last_handled, seq_num) && 0 <= uxr_seq_num_cmp(last_history, seq_num))
    {
        /* Process the message */
        FragmentationInfo fragmentation_info = uxr_get_fragmentation_info(buffer);
        uxrSeqNum next = uxr_seq_num_add(stream->last_handled, 1);

        if((NO_FRAGMENTED == fragmentation_info) && (seq_num == next))
        {
            stream->last_handled = next;
            ready_to_read = true;
            *message_stored = false;
        }
        else
        {
            /* Check if the message received is not already received */
            uint8_t* internal_buffer = uxr_get_input_buffer(stream, seq_num % stream->history);
            if(0 == uxr_get_reliable_buffer_length(internal_buffer))
            {
                memcpy(internal_buffer + INTERNAL_RELIABLE_BUFFER_OFFSET, buffer, length);
                uxr_set_reliable_buffer_length(internal_buffer, length);
                *message_stored = true;

                if(NO_FRAGMENTED != fragmentation_info)
                {
                    uxrSeqNum last;
                    if(check_last_fragment(stream, &last))
                    {
                        ready_to_read = true;
                    }
                }
            }
        }
    }

    if(0 > uxr_seq_num_cmp(stream->last_announced, seq_num))
    {
        stream->last_announced = seq_num;
    }

    return ready_to_read;
}

bool uxr_next_input_reliable_buffer_available(
        uxrInputReliableStream* stream,
        ucdrStream* us)
{
    uxrSeqNum next = uxr_seq_num_add(stream->last_handled, 1);
    uint8_t* internal_buffer = uxr_get_input_buffer(stream, next % stream->history);
    size_t length = uxr_get_reliable_buffer_length(internal_buffer);
    bool available_to_read = (0 != length);
    if(available_to_read)
    {
        FragmentationInfo fragmentation_info = uxr_get_fragmentation_info(internal_buffer + INTERNAL_RELIABLE_BUFFER_OFFSET);
        if(NO_FRAGMENTED == fragmentation_info)
        {
            ucdr_init_stream(us, internal_buffer + INTERNAL_RELIABLE_BUFFER_OFFSET, length);
            uxr_set_reliable_buffer_length(internal_buffer, 0);
            stream->last_handled = next;
        }
        else
        {
            uxrSeqNum last;
            available_to_read = check_last_fragment(stream, &last);
            if(available_to_read)
            {
                memcpy(stream->buffer, internal_buffer + INTERNAL_RELIABLE_BUFFER_OFFSET + SUBHEADER_SIZE, length - SUBHEADER_SIZE);
                uxr_set_reliable_buffer_length(internal_buffer, 0);
                stream->offset = length - SUBHEADER_SIZE;
                do
                {
                    next = uxr_seq_num_add(next, 1);
                    internal_buffer = uxr_get_input_buffer(stream, next % stream->history);
                    length = uxr_get_reliable_buffer_length(internal_buffer);

                    if ((length + stream->offset) > stream->max_message_size)
                    {
                        stream->last_handled = last;
                        available_to_read = false;
                        break;
                    }

                    memcpy(stream->buffer + stream->offset, internal_buffer + INTERNAL_RELIABLE_BUFFER_OFFSET + SUBHEADER_SIZE, length - SUBHEADER_SIZE);
                    uxr_set_reliable_buffer_length(internal_buffer, 0);
                    stream->offset += (length - SUBHEADER_SIZE);

                } while (last != next);
                stream->last_handled = last;

                if (available_to_read)
                {
                    ucdr_init_stream(us, stream->buffer, stream->offset);
                }
            }
        }
    }

    return available_to_read;
}

void uxr_process_heartbeat(
        uxrInputReliableStream* stream,
        uxrSeqNum first_seq_num,
        uxrSeqNum last_seq_num)
{
    (void)first_seq_num;
    //TODO: Checks the first_seq_num to avoid hacks.

    if(0 > uxr_seq_num_cmp(stream->last_announced, last_seq_num))
    {
        stream->last_announced = last_seq_num;
    }
}

bool uxr_is_input_up_to_date(
        const uxrInputReliableStream* stream)
{
    return stream->last_announced == stream->last_handled;
}

uint16_t uxr_compute_acknack(
        const uxrInputReliableStream* stream,
        uxrSeqNum* from)
{
    *from = uxr_get_first_unacked(stream);
    uint16_t buffers_to_ack = uxr_seq_num_sub(stream->last_announced, uxr_seq_num_sub(*from, 1));
    uint16_t nack_bitmap = 0;

    for(size_t i = 0; i < buffers_to_ack; ++i)
    {
        uxrSeqNum seq_num = uxr_seq_num_add(*from, (uxrSeqNum)i);
        uint8_t* internal_buffer = uxr_get_input_buffer(stream, seq_num % stream->history);
        if(0 == uxr_get_reliable_buffer_length(internal_buffer))
        {
            nack_bitmap = (uint16_t)(nack_bitmap | (1 << i));
        }
    }

    return nack_bitmap;
}

uint8_t* uxr_get_input_buffer(
        const uxrInputReliableStream* stream,
        size_t history_pos)
{
    return stream->buffer + stream->max_message_size + (stream->max_fragment_size * history_pos);
}

//==================================================================
//                             PRIVATE
//==================================================================
bool check_last_fragment(
        uxrInputReliableStream* stream,
        uxrSeqNum* last_fragment)
{
    uxrSeqNum next = stream->last_handled;
    bool more_messages;
    bool found = false;
    do
    {
        next = uxr_seq_num_add(next, 1);
        uint8_t* next_buffer = uxr_get_input_buffer(stream, next % stream->history);
        more_messages = (0 != uxr_get_reliable_buffer_length(next_buffer));
        if(more_messages)
        {
            FragmentationInfo next_fragmentation_info = uxr_get_fragmentation_info(next_buffer + INTERNAL_RELIABLE_BUFFER_OFFSET);
            more_messages = (INTERMEDIATE_FRAGMENT == next_fragmentation_info);
            if(LAST_FRAGMENT == next_fragmentation_info)
            {
                found = true;
                break;
            }
        }
    }
    while(more_messages);

    *last_fragment = next;
    return found;
}

uxrSeqNum uxr_get_first_unacked(
        const uxrInputReliableStream* stream)
{
    uxrSeqNum first_unknown = stream->last_handled;
    for(size_t i = 0; i < stream->history; ++i)
    {
        uxrSeqNum seq_num = uxr_seq_num_add(stream->last_handled, (uint16_t)(i + 1));
        uint8_t* buffer = uxr_get_input_buffer(stream, seq_num % stream->history);
        if(0 == uxr_get_reliable_buffer_length(buffer))
        {
            first_unknown = seq_num;
            break;
        }
    }

    return first_unknown;
}

FragmentationInfo uxr_get_fragmentation_info(
        uint8_t* buffer)
{
    ucdrStream us;
    ucdr_init_stream(&us, buffer, SUBHEADER_SIZE);

    uint8_t id; uint16_t length; uint8_t flags;
    uxr_read_submessage_header(&us, &id, &length, &flags);

    FragmentationInfo fragmentation_info;
    if(SUBMESSAGE_ID_FRAGMENT == id)
    {
        fragmentation_info = FLAG_LAST_FRAGMENT & flags ? LAST_FRAGMENT : INTERMEDIATE_FRAGMENT;
    }
    else
    {
        fragmentation_info = NO_FRAGMENTED;
    }
    return fragmentation_info;
}
