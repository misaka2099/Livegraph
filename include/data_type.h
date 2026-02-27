#pragma once 
#define ATTR_CACHEABLE 

#define AP_INT_MAX_W 2048 
#include "ap_int.h"
#include "ap_axi_sdata.h"
// typedef  ap_uint<8> BYTE_STREAM;
#define CHANNEL_CAPACITY (250*1024*1024)
#define CHANNEL_NUM 15
typedef ap_axiu<8,0,0,0> BYTE_STREAM ;
typedef ap_axiu<32,3,0,0> COMMAND_STREAM;
typedef ap_uint<32> COMMAND;
typedef ap_uint<8> BYTE;
// typedef double ATTR_STREAM;
#define PMA_STOP 0
#define PMA_INSERT_EDGE 1 
#define PMA_INSERT_VERTEX 2 
#define PMA_GET_OUT_EDGE 3
#define PMA_GET_IN_EDGE 4 
#define PMA_GET_ATTR 5 
#define PMA_CHANGE_ATTR 6 
#define MEM_WRITE 7
#define MEM_READ 8 
#define MEM_STOP 9
#define MEM_ALLOC 10
#define MEM_FREE 11
#define MEM_COPY 12
#define PMA_GET_ALL_OUT_EDGE_COO 17
#define PMA_GET_ALL_IN_EDGE_COO 18
#define PMA_GET_ALL_OUT_EDGE_CSR 19
#define PMA_GET_ALL_IN_EDGE_CSR 20
#define INIT_VEC_LEN_IN_BYTE 1024
//64 elements
#define SEGMENT_SIZE 64 
#define DENSITY_BOUND 0.75
//0-31 start address
//32-63 length
typedef ap_uint<64> VEC_INFO;
typedef ap_axiu<64, 3, 0, 0> VEC_INFO_STREAM;
//0-31: start position
//32-63: length
// typedef ap_uint<64> ACCESS_POS ;
#define VERTEX_PER_PMA 16
#define UNUSED_ICON -1
#define COMMAND_BUFFER_SIZE 8
#define WRITE_KERNEL_BUFFER_SIZE 64
#define STOP_ICON 
typedef ap_axiu<72, 0, 0, 0> TAG_STREAM; // 64 VECINFO + 8tag 
#define READER_CHUNK_BYTES 512
// (0,31) src (32,63) dst or (32, 95) attr (96,97) type(insert vertex, insert edge, change attr) (98)direction()
typedef ap_axiu<128, 0, 0, 0> PMA_UPDATE_MESSAGE; 
#define MAX_PMA_NUM 1000000

#define OUT_DIRECTION 0
#define IN_DIRECTION 1
typedef ap_axiu<64,0,0,0> READ_COMMAND_STREAM; 
typedef  ap_axiu<8,0,0,0> CLEAR_ICON;



#define VERTEX_ATTR_SIZE_IN_BYTE 16
typedef ap_uint<64> VERTEX_ATTR_TYPE; 
struct VERTEX_ATTR_STREAM_TYPE {
    VERTEX_ATTR_TYPE data;
    ap_uint<1> last;
};
#ifdef ATTR_CACHEABLE
struct alignas(64)PMA_HEADER
{
    int vertex_range[VERTEX_PER_PMA];//every vertex in PMA's end segment [)
    int edge_count[VERTEX_PER_PMA];//every vertex in PMA's edge count
    VERTEX_ATTR_TYPE attr[VERTEX_PER_PMA];//every vertex in PMA's attr
};
#else
struct alignas(64)PMA_HEADER
{
    int vertex_range[VERTEX_PER_PMA];//every vertex in PMA's end segment [)
    int edge_count[VERTEX_PER_PMA];//every vertex in PMA's edge count
    ap_uint<64> attr[VERTEX_PER_PMA];//every vertex in PMA's attr
};
#endif
typedef ap_uint<512> memory_chunk; 

struct INTRA_PMA_UPDATE_MESSAGE
{
    ap_uint<128> data;
    ap_uint<1> last;
};



#define MAX_COMMAND_BUFFER_LENGTH 16
#define MAX_COMMAND_BUFFER_NUM 16
#define MAX_CMD_NUM 16
#define LOAD_BITMAP 19

struct pma_idx_info
{
    int pma_idx;
    int direction;
    int last;
};