#include "data_type.h"
#include "debug.h"
#include <hls_stream.h>
extern "C"
{
    void signal_kernel(
        hls::stream<COMMAND_STREAM> &signal_to_mem,
        int signal
    )
    {
        #pragma HLS INTERFACE axis port=signal_to_mem
        #pragma HLS INTERFACE s_axilite port=signal 
        signal_to_mem.write(COMMAND_STREAM{.data = signal});
    }
}