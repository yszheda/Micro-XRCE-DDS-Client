#include "output_best_effort_stream_internal.h"

#include "../submessage_internal.h"
#include "seq_num_internal.h"

#include <ucdr/microcdr.h>

//==================================================================
//                              PUBLIC
//==================================================================
void uxr_init_output_best_effort_stream(
        uxrOutputBestEffortStream* stream,
        uint8_t* buffer,
        size_t size,
        size_t header_offset)
{
    stream->buffer = buffer;
    stream->size = size;
    stream->header_offset = header_offset;

    uxr_reset_output_best_effort_stream(stream);
}

void uxr_reset_output_best_effort_stream(
        uxrOutputBestEffortStream* stream)
{
    stream->offset = stream->header_offset;
    stream->last_send = SEQ_NUM_MAX;
}

bool uxr_prepare_best_effort_buffer_to_write(
        uxrOutputBestEffortStream* stream,
        size_t submessage_size,
        ucdrStream* us)
{
    size_t future_offset = stream->offset + uxr_submessage_padding(stream->offset) + submessage_size;
    bool available_to_write = future_offset <= stream->size;
    if(available_to_write)
    {
        ucdr_init_stream_offset(us, stream->buffer, future_offset, stream->offset);
        stream->offset = future_offset;
    }
    return available_to_write;
}

bool uxr_prepare_best_effort_buffer_to_send(
        uxrOutputBestEffortStream* stream,
        uint8_t** buffer,
        size_t* length,
        uint16_t* seq_num)
{
    bool data_to_send = stream->header_offset > stream->offset;
    if(data_to_send)
    {
        stream->last_send = uxr_seq_num_add(stream->last_send, 1);

        *seq_num = stream->last_send;
        *buffer = stream->buffer;
        *length = stream->offset;

        stream->offset = stream->header_offset;
    }

    return data_to_send;
}

