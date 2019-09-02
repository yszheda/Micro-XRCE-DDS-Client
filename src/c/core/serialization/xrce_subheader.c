#include "xrce_subheader_internal.h"

//==================================================================
//                             PUBLIC
//==================================================================
void uxr_serialize_submessage_header(
        ucdrStream* us,
        uint8_t id,
        uint8_t flags,
        uint16_t length)
{
    (void) ucdr_serialize_uint8_t(us, id);
    (void) ucdr_serialize_uint8_t(us, flags);
    (void) ucdr_serialize_endian_uint16_t(us, UCDR_LITTLE_ENDIANNESS, length);
}

void uxr_deserialize_submessage_header(
        ucdrStream* us,
        uint8_t* id,
        uint8_t* flags,
        uint16_t* length)
{
    (void) ucdr_deserialize_uint8_t(us, id);
    (void) ucdr_deserialize_uint8_t(us, flags);
    (void) ucdr_deserialize_endian_uint16_t(us, UCDR_LITTLE_ENDIANNESS, length);
}
