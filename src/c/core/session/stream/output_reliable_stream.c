#include "output_reliable_stream_internal.h"

#include <uxr/client/config.h>
#include <string.h>

#include "seq_num_internal.h"
#include "output_reliable_stream_internal.h"
#include "common_reliable_stream_internal.h"
#include "../submessage_internal.h"
#include <ucdr/microcdr.h>

#define MIN_HEARTBEAT_TIME_INTERVAL ((int64_t) UXR_CONFIG_MIN_HEARTBEAT_TIME_INTERVAL) // ms
#define MAX_HEARTBEAT_TRIES         (sizeof(int64_t) * 8 - 1)

//==================================================================
//                             PUBLIC
//==================================================================
void uxr_init_output_reliable_stream(
        uxrOutputReliableStream* stream,
        uint8_t* buffer,
        size_t max_message_size,
        size_t max_fragment_size,
        uint16_t history,
        uint8_t header_offset)
{
    // assert for history (must be 2^)

    stream->buffer = buffer;
    stream->max_message_size = max_message_size;
    stream->max_fragment_size = max_fragment_size;
    stream->history = history;
    stream->header_offset = header_offset;

    uxr_reset_output_reliable_stream(stream);
}

void uxr_reset_output_reliable_stream(
        uxrOutputReliableStream* stream)
{
    for(size_t i = 0; i < stream->history; i++)
    {
        uint8_t* buffer = uxr_get_output_buffer(stream, i);
        uxr_set_reliable_buffer_length(buffer, 0);
    }

    stream->offset = 0;
    stream->iterator = 0;

    stream->last_sent = SEQ_NUM_MAX;
    stream->last_acknown = SEQ_NUM_MAX;

    stream->next_heartbeat_timestamp = INT64_MAX;
    stream->next_heartbeat_tries = 0;
    stream->send_lost = false;
}

bool uxr_prepare_reliable_buffer_to_write(
        uxrOutputReliableStream* stream,
        size_t submessage_size,
        ucdrStream* us)
{
    bool rv = false;
    submessage_size += ucdr_alignment(stream->offset, 4);
    if ((0 == stream->iterator) && (submessage_size + stream->offset <= stream->max_message_size))
    {
        rv = true;
        ucdr_init_stream_offset(us, stream->buffer, submessage_size + stream->offset, stream->offset);
        stream->offset += submessage_size;
    }
    return rv;
}

bool uxr_prepare_next_reliable_buffer_to_send(
        uxrOutputReliableStream* stream,
        uint8_t** buffer,
        size_t* length,
        uxrSeqNum* seq_num)
{
    bool rv = false;
    const uxrSeqNum next = uxr_seq_num_add(stream->last_sent, 1);
    if ((0 != stream->offset) && (0 != uxr_seq_num_sub(stream->last_acknown, next) % stream->history))
    {
        rv = true;
        *buffer = uxr_get_output_buffer(stream, *seq_num % stream->history);
        const uint8_t* data = stream->buffer + stream->iterator;
        size_t data_size;
        size_t data_offset;

        if ((0 == stream->iterator) && (stream->offset <= stream->max_fragment_size))
        {
            data_size = stream->offset;
            data_offset = stream->header_offset;
            stream->offset = 0;
        }
        else
        {
            data_size = (stream->offset - stream->iterator);
            data_offset = (size_t)(stream->header_offset + SUBHEADER_SIZE);
            uint8_t flag = 0;
            if (stream->max_fragment_size >= (data_offset + data_size))
            {
                stream->iterator = 0;
                stream->offset = 0;
                flag = FLAG_LAST_FRAGMENT;
            }
            else
            {
                data_size = stream->max_fragment_size - data_offset;
                stream->iterator += data_size;
            }
            ucdrStream us;
            ucdr_init_stream_offset(&us, *buffer, stream->max_fragment_size, stream->header_offset);
            uxr_buffer_submessage_header(&us, SUBMESSAGE_ID_FRAGMENT, (uint16_t)(data_size), flag);
        }
        stream->last_sent = next;
        *seq_num = stream->last_sent;
        *length = data_offset + data_size;
        uxr_set_reliable_buffer_length(*buffer, *length);
        memcpy(*buffer + data_offset, data, data_size);
    }
    return rv;
}

bool uxr_update_output_stream_heartbeat_timestamp(
        uxrOutputReliableStream* stream,
        int64_t current_timestamp)
{
    bool must_confirm = false;
    if(0 > uxr_seq_num_cmp(stream->last_acknown, stream->last_sent))
    {
        if(0 == stream->next_heartbeat_tries)
        {
            stream->next_heartbeat_timestamp = current_timestamp + MIN_HEARTBEAT_TIME_INTERVAL;
            stream->next_heartbeat_tries = 1;
        }
        else if(current_timestamp >= stream->next_heartbeat_timestamp)
        {
            int64_t increment = MIN_HEARTBEAT_TIME_INTERVAL << (stream->next_heartbeat_tries % MAX_HEARTBEAT_TRIES);
            int64_t difference = current_timestamp - stream->next_heartbeat_timestamp;
            stream->next_heartbeat_timestamp += (difference > increment) ? difference : increment;
            stream->next_heartbeat_tries++;
            must_confirm = true;
        }
    }
    else
    {
        stream->next_heartbeat_timestamp = INT64_MAX;
    }

    return must_confirm;
}

uxrSeqNum uxr_begin_output_nack_buffer_it(
        const uxrOutputReliableStream* stream)
{
    return stream->last_acknown;
}

bool uxr_next_reliable_nack_buffer_to_send(
        uxrOutputReliableStream* stream,
        uint8_t** buffer,
        size_t *length,
        uxrSeqNum* seq_num_it)
{
    bool it_updated = false;
    if(stream->send_lost)
    {
        bool check_next_buffer = true;
        while(check_next_buffer && !it_updated)
        {
            *seq_num_it = uxr_seq_num_add(*seq_num_it, 1);
            check_next_buffer = 0 >= uxr_seq_num_cmp(*seq_num_it, stream->last_sent);
            if(check_next_buffer)
            {
                *buffer = uxr_get_output_buffer(stream, *seq_num_it % stream->history);
                *length = uxr_get_reliable_buffer_length(*buffer);
                it_updated = (*length != 0);
            }
        }

        if(!it_updated)
        {
            stream->send_lost = false;
        }
    }

    return it_updated;
}

void uxr_process_acknack(
        uxrOutputReliableStream* stream,
        uint16_t bitmap,
        uxrSeqNum first_unacked_seq_num)
{
    uxrSeqNum last_acked_seq_num = uxr_seq_num_sub(first_unacked_seq_num, 1);
    size_t buffers_to_clean = uxr_seq_num_sub(last_acked_seq_num, stream->last_acknown);
    for(size_t i = 0; i < buffers_to_clean; i++)
    {
        stream->last_acknown = uxr_seq_num_add(stream->last_acknown, 1);
        uint8_t* internal_buffer = uxr_get_output_buffer(stream, stream->last_acknown % stream->history);
        uxr_set_reliable_buffer_length(internal_buffer, 0); /* clear buffer */
    }

    stream->send_lost = (0 < bitmap);

    /* reset heartbeat interval */
    stream->next_heartbeat_tries = 0;
}

bool uxr_is_output_up_to_date(
        const uxrOutputReliableStream* stream)
{
    return 0 == uxr_seq_num_cmp(stream->last_acknown, stream->last_sent);
}

uint8_t* uxr_get_output_buffer(
        const uxrOutputReliableStream* stream,
        size_t history_pos)
{
    return stream->buffer + stream->max_message_size + (stream->max_fragment_size * history_pos);
}
