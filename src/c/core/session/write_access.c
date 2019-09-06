#include <uxr/client/core/session/write_access.h>

#include "session_internal.h"
#include "session_info_internal.h"
#include "submessage_internal.h"
#include "../serialization/xrce_protocol_internal.h"

#define WRITE_DATA_PAYLOAD_SIZE 4

//==================================================================
//                             PUBLIC
//==================================================================
bool uxr_prepare_output_stream(
        uxrSession* session,
        uxrStreamId stream_id,
        uxrObjectId datawriter_id,
        ucdrStream* us,
        uint32_t topic_size)
{
    size_t payload_size = WRITE_DATA_PAYLOAD_SIZE + topic_size;
    us->error = !uxr_prepare_stream_to_write_submessage(session, stream_id, payload_size, us, SUBMESSAGE_ID_WRITE_DATA, FORMAT_DATA);
    if(!us->error)
    {
        WRITE_DATA_Payload_Data payload;
        uxr_init_base_object_request(&session->info, datawriter_id, &payload.base);
        (void) uxr_serialize_WRITE_DATA_Payload_Data(us, &payload);

        ucdr_init_stream(us, us->buffer_info.data + us->offset, us->buffer_info.size - us->offset);
    }

    return !us->error;
}

