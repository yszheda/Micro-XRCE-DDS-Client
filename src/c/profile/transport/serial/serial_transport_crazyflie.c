#include <uxr/client/profile/transport/serial/serial_transport.h>
#include <uxr/client/profile/transport/serial/serial_transport_crazyflie.h>
#include "serial_transport_internal.h"

#include <unistd.h>
#include <errno.h>

#include <crtp.h>

#include <syslink.h>
#include <radiolink.h>
#include "string.h"



bool uxr_init_serial_platform(struct uxrSerialPlatform* platform, int fd, uint8_t remote_addr, uint8_t local_addr)
{
    //TODO: For this RTOS is not necessary to initialize nothing here
    return true;
}

bool uxr_close_serial_platform(struct uxrSerialPlatform* platform)
{
    //TODO Is not necessary to close or open the platform on this fork of freeRTOS
    return true;
}

size_t uxr_write_serial_data_platform(uxrSerialPlatform* platform, uint8_t* buf, size_t len, uint8_t* errcode)
{
  //First we check the size of the message
  static CRTPPacket send_pkg;
  send_pkg.header = CRTP_HEADER(8, 0);
  int index_send=0;
  int i=0;

  while(1){
    send_pkg.data[i]=buf[index_send];
    if(i==CRTP_MAX_DATA_SIZE-1){
      send_pkg.size=CRTP_MAX_DATA_SIZE;
      crtpSendPacket(&send_pkg);
      i=0;
      index_send++;
    }
    else if(index_send==len-1){
      i++;
      send_pkg.size=i;
      crtpSendPacket(&send_pkg);
      break;
    }
    else{
      i++;
      index_send++;
    }
  }

  return index_send+1;

}

size_t uxr_read_serial_data_platform(uxrSerialPlatform* platform, uint8_t* buf, size_t len, int timeout, uint8_t* errcode)
{
  static CRTPPacket recv_packet;
  size_t rv = 0;
  int data_rdy = crtpReceivePacketWait(8, &recv_packet, timeout);

  if(data_rdy == 1){

    memcpy(buf,recv_packet.data,sizeof(recv_packet.data));
    rv = (size_t)recv_packet.size;
    *errcode = 0;
  }
  else{
    *errcode = 1;
  }

    return rv;
}
