#include <uxr/client/util/time.h>
#include <time.h>


//==================================================================
//                             PUBLIC
//==================================================================
int64_t uxr_millis(void)
{
  uint64_t time = usecTimestamp();
  return time/1000;
}

int64_t uxr_nanos(void)
{
  uint64_t time = usecTimestamp();
  return time * 1000;
}
