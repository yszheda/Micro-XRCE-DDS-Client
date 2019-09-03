#include "submessage_internal.h"
#include "../serialization/xrce_subheader_internal.h"

//==================================================================
//                             PUBLIC
//==================================================================
bool uxr_buffer_submessage_header(
        ucdrStream* us,
        uint8_t submessage_id,
        uint16_t length,
        uint8_t flags)
{
    ucdr_align(us, 4);
    us->endianness = UCDR_MACHINE_ENDIANNESS;
    flags = (uint8_t)(flags | us->endianness);
    uxr_serialize_submessage_header(us, submessage_id, flags, length);

    return ucdr_remaining_size(us) >= length;
}

bool uxr_read_submessage_header(
        ucdrStream* us,
        uint8_t* submessage_id,
        uint16_t* length,
        uint8_t* flags)
{
    ucdr_align(us, 4);
    bool ready_to_read = ucdr_remaining_size(us) >= SUBHEADER_SIZE;
    if(ready_to_read)
    {
        uxr_deserialize_submessage_header(us, submessage_id, flags, length);

        uint8_t endiannes_flag = *flags & FLAG_ENDIANNESS;
        *flags = (uint8_t)(*flags & ~endiannes_flag);
        us->endianness = endiannes_flag ? UCDR_LITTLE_ENDIANNESS : UCDR_BIG_ENDIANNESS;
    }

    return ready_to_read;
}

size_t uxr_submessage_padding(
        size_t length)
{
    return (length % SUBHEADER_SIZE != 0) ? SUBHEADER_SIZE - (length % SUBHEADER_SIZE) : 0;
}
