#include "data_type.h"
#include "debug.h"
#include <cstdio>
#include <cstring>
#include <hls_stream.h>
#include <ap_int.h> 

int read_edge(
    int src,
    PMA_HEADER &pma_header,
    int header_start,
    ap_uint<512>* hbm_ptr,
    hls::stream<ap_axiu<32,0,0,0>> &nei_stream,
    hls::stream<ap_axiu<64,0,0,0>> &coo_stream,
    hls::stream<ap_axiu<32,0,0,0>> &csr_col_stream,
    int mode
)
{
    int local_src = src % VERTEX_PER_PMA;
    int start_seg_offset = local_src == 0 ? 0 : pma_header.vertex_range[local_src - 1];
    int src_seg = pma_header.vertex_range[local_src] - start_seg_offset;
    int edge_start = header_start + sizeof(PMA_HEADER) + start_seg_offset * sizeof(int) * SEGMENT_SIZE;
    int edge_len = src_seg * sizeof(int) * SEGMENT_SIZE;
    
    int aligned_len = (edge_len + 63) & ~0x3F;
    if (aligned_len == 0)
    {
        if (mode == 0)
        {
            ap_axiu<32,0,0,0> nei_pkt = {};
            nei_pkt.data = (ap_uint<32>)(-1);
            nei_pkt.keep = (ap_uint<4>)0xF;
            nei_pkt.strb = (ap_uint<4>)0xF;
            nei_pkt.last = 1;
            nei_stream.write(nei_pkt);
        }
        if (mode == 1)
        {
            // ap_axiu<64,0,0,0> coo_pkt = {};
            // coo_pkt.data = ((ap_uint<64>)src << 32) | (ap_uint<32>)(-1);
            // coo_pkt.keep = (ap_uint<8>)0xFF;
            // coo_pkt.strb = (ap_uint<8>)0xFF;
            // coo_pkt.last = 1;
            // coo_stream.write(coo_pkt);
        }
        if (mode == 2)
        {
            // ap_axiu<32,0,0,0> csr_pkt = {};
            // csr_pkt.data = (ap_uint<32>)-1;
            // csr_pkt.keep = (ap_uint<8>)0xFF;
            // csr_pkt.strb = (ap_uint<8>)0xFF;
            // csr_pkt.last = 1;
            // csr_col_stream.write(csr_pkt);
        }
        return 0;
    }

    int first_word = edge_start >> 6;
    int word_count = aligned_len >> 6;
    ap_uint<512> segment_buffer[4];
    int csr_count = 0;
    for (int seg = 0 ; seg < src_seg ; seg++)
    {
        #pragma HLS PIPELINE II=1
        for (int i = 0 ; i < 4 ; i++)
        {
            #pragma HLS UNROLL
            segment_buffer[i] = hbm_ptr[first_word + seg * 4 + i];
        }
        for (int i = 0 ; i < SEGMENT_SIZE; i++)
        {
            #pragma HLS PIPELINE II=1
            int *segment_ptr = (int*)&segment_buffer;
            int neighbor = segment_ptr[i];
            if (neighbor == UNUSED_ICON)
            {
                break;
            }
            if (mode == 0)
            {
                ap_axiu<32,0,0,0> nei_pkt = {};
                nei_pkt.data = (ap_uint<32>)neighbor;
                nei_pkt.keep = (ap_uint<4>)0xF;
                nei_pkt.strb = (ap_uint<4>)0xF;
                nei_pkt.last = 0;
                nei_stream.write(nei_pkt);
            }
            if (mode == 1)
            {
                ap_axiu<64,0,0,0> coo_pkt = {};
                coo_pkt.data = ((ap_uint<64>)src << 32) | (ap_uint<32>)neighbor;
                coo_pkt.keep = (ap_uint<8>)0xFF;
                coo_pkt.strb = (ap_uint<8>)0xFF;
                coo_pkt.last = 0;
                coo_stream.write(coo_pkt);
            }
            if (mode == 2)
            {
                ap_axiu<32,0,0,0> csr_col_pkt = {};
                csr_col_pkt.data = ((ap_uint<32>)neighbor);
                csr_col_pkt.keep = (ap_uint<8>)0xFF;
                csr_col_pkt.strb = (ap_uint<8>)0xFF;
                csr_col_pkt.last = 0;
                csr_col_stream.write(csr_col_pkt);
                csr_count++;
            }
        }
    }

    if (mode == 0)
    {
        ap_axiu<32,0,0,0> nei_end = {};
        nei_end.data = (ap_uint<32>)(-1);
        nei_end.keep = (ap_uint<4>)0xF;
        nei_end.strb = (ap_uint<4>)0xF;
        nei_end.last = 1;
        nei_stream.write(nei_end);
    }
    if (mode == 1)
    {
        // ap_axiu<64,0,0,0> coo_end = {};
        // coo_end.data = ((ap_uint<64>)src << 32) | (ap_uint<32>)(-1);
        // coo_end.keep = (ap_uint<8>)0xFF;
        // coo_end.strb = (ap_uint<8>)0xFF;
        // coo_end.last = 1;
        // coo_stream.write(coo_end);
    }
    if (mode == 2)
    {
        // ap_axiu<64,0,0,0> csr_summary = {};
        // csr_summary.data = ((ap_uint<64>)csr_row_start << 32) | (ap_uint<32>)(csr_row_start + csr_count);
        // csr_summary.keep = (ap_uint<8>)0xFF;
        // csr_summary.strb = (ap_uint<8>)0xFF;
        // csr_summary.last = 1;
        // csr_col_stream.write(csr_summary);
    }
    return csr_count;
}

void read_header(
    int start_addr,
    ap_uint<512>* hbm_ptr,
    PMA_HEADER &pma_header
)
{
    const int header_word_offset = start_addr >> 6;
    const int header_word_count = sizeof(PMA_HEADER) >> 6;
    ap_uint<512>* header_ptr = reinterpret_cast<ap_uint<512>*>(&pma_header);
    for (int i = 0; i < header_word_count; ++i)
    {
        #pragma HLS PIPELINE II=1
        header_ptr[i] = hbm_ptr[header_word_offset + i];
    }
}
extern "C"
{
    void pma_read_kernel(
        VEC_INFO* pos_pma_addrs,
        VEC_INFO* neg_pma_addrs,
        ap_uint<512>* hbm_ptr,
        hls::stream<READ_COMMAND_STREAM> &cmd_stream,
        hls::stream<ap_axiu<32,0,0,0>> &data_to_nei_query,
        hls::stream<ap_axiu<64,0,0,0>> &data_to_coo_stream,
        hls::stream<ap_axiu<32,0,0,0>> &data_to_csr_col_stream,
        hls::stream<ap_axiu<32,0,0,0>> &data_to_csr_row_ptr_stream,
        hls::stream<ap_axiu<64,0,0,0>> &data_to_attr_query
        //memory
        // hls::stream<VEC_INFO_STREAM> &vec_info_stream,
        // hls::stream<COMMAND_STREAM> &cmd_to_mem_stream,
        // hls::stream<BYTE_STREAM> &data_from_mem_stream,
        // hls::stream<VEC_INFO_STREAM> &vec_info_from_mem,
        // hls::stream<BYTE_STREAM> &data_to_mem_stream
        // hls::stream<COMMAND_STREAM> &stop_signal
    )
    {
        #pragma HLS INTERFACE m_axi port=pos_pma_addrs offset=slave bundle=gmem0
        #pragma HLS INTERFACE m_axi port=neg_pma_addrs offset=slave bundle=gmem1
        #pragma HLS INTERFACE axis port=cmd_stream
        #pragma HLS INTERFACE axis port=data_to_nei_query
        #pragma HLS INTERFACE axis port=data_to_coo_stream
        #pragma HLS INTERFACE axis port=data_to_csr_col_stream
        #pragma HLS INTERFACE axis port=data_to_csr_row_ptr_stream
        #pragma HLS INTERFACE axis port=data_to_attr_query
        #pragma HLS INTERFACE m_axi port=hbm_ptr offset=slave bundle=gmem2 depth=256 max_widen_bitwidth=512
        while (true)
        {
            READ_COMMAND_STREAM cmd = cmd_stream.read();
            // HERE
            if (cmd.data == PMA_STOP && cmd.last == 1) 
            {
                break;
            }
            switch (cmd.data.range(31,0)) 
            {
                case PMA_GET_OUT_EDGE:
                {
                    // HERE
                    int src = cmd.data.range(63,32);
                    int pma_idx = src / VERTEX_PER_PMA;
                    VEC_INFO vec_info;
                    vec_info = pos_pma_addrs[pma_idx];
                    int start_addr = vec_info.range(31,0);
                    int pma_len = vec_info.range(63,32);
                    if (vec_info == 0) // empty PMA
                    {
                        data_to_nei_query.write({.data=-1});
                        break;
                    }
                    PMA_HEADER pma_header;
                    HERE
                    read_header(start_addr, hbm_ptr, pma_header);
                    read_edge(
                        src,
                        pma_header,
                        vec_info.range(31,0),
                        hbm_ptr,
                        data_to_nei_query,
                        data_to_coo_stream,
                        data_to_csr_col_stream,
                        0
                    );
                    HERE
                    break;
                }
                case PMA_GET_IN_EDGE:
                {
                    int src = cmd.data.range(63,32);
                    int pma_idx = src / VERTEX_PER_PMA;
                    VEC_INFO vec_info;
                    vec_info = neg_pma_addrs[pma_idx];
                    int start_addr = vec_info.range(31,0);
                    int pma_len = vec_info.range(63,32);
                    if (vec_info == 0) // empty PMA
                    {
                        data_to_nei_query.write({.data=-1});
                        break;
                    }
                    PMA_HEADER pma_header;
                    read_header(start_addr, hbm_ptr, pma_header);
                    
                    read_edge(
                        src,
                        pma_header,
                        vec_info.range(31,0),
                        hbm_ptr,
                        data_to_nei_query,
                        data_to_coo_stream,
                        data_to_csr_col_stream,
                        0
                    );
                    break;
                }
                case PMA_GET_ATTR:
                {
                    int vertex = cmd.data.range(63,32);
                    int pma_idx = vertex / VERTEX_PER_PMA;
                    VEC_INFO vec_info;
                    vec_info = pos_pma_addrs[pma_idx];
                    if (vec_info == 0) // empty PMA
                    {
                        data_to_attr_query.write({.data=-1});
                    }
                    PMA_HEADER pma_header;
                    int start_addr = vec_info.range(31,0);
                    const int header_word_offset = start_addr >> 6;
                    const int header_word_count = sizeof(PMA_HEADER) >> 6;
                    ap_uint<512>* header_ptr = reinterpret_cast<ap_uint<512>*>(&pma_header);
                    for (int i = 0; i < header_word_count; ++i)
                    {
                        #pragma HLS PIPELINE II=1
                        header_ptr[i] = hbm_ptr[header_word_offset + i];
                    }
                    double attr = pma_header.attr[vertex % VERTEX_PER_PMA];
                    data_to_attr_query.write({.data=attr});
                    break;
                }
                case PMA_GET_ALL_OUT_EDGE_COO:
                {
                    for (int pma_idx = 0; pma_idx < MAX_PMA_NUM; pma_idx++)
                    {
                        VEC_INFO vec_info = pos_pma_addrs[pma_idx];
                        if (vec_info == 0) break;
                        PMA_HEADER pma_header;
                        read_header(vec_info.range(31,0), hbm_ptr, pma_header);
                        for (int local_v = 0; local_v < VERTEX_PER_PMA; local_v++)
                        {
                            int src = pma_idx * VERTEX_PER_PMA + local_v;
                            read_edge(src, pma_header, vec_info.range(31,0), hbm_ptr, data_to_nei_query,
                        data_to_coo_stream,
                        data_to_csr_col_stream,
                        1);
                        }
                    }
                    data_to_coo_stream.write({.data=-1, .last=1});
                    break;
                }
                case PMA_GET_ALL_IN_EDGE_COO:
                {
                    for (int pma_idx = 0; pma_idx < MAX_PMA_NUM; pma_idx++)
                    {
                        VEC_INFO vec_info = neg_pma_addrs[pma_idx];
                        if (vec_info == 0) break;
                        PMA_HEADER pma_header;
                        read_header(vec_info.range(31,0), hbm_ptr, pma_header);
                        for (int local_v = 0; local_v < VERTEX_PER_PMA; local_v++)
                        {
                            int src = pma_idx * VERTEX_PER_PMA + local_v;
                            read_edge(src, pma_header, vec_info.range(31,0), hbm_ptr, data_to_nei_query,
                        data_to_coo_stream,
                        data_to_csr_col_stream,
                        1);
                        }
                    }
                    data_to_coo_stream.write({.data=-1, .last=1});
                    break;
                }
                case PMA_GET_ALL_OUT_EDGE_CSR:
                {
                    ap_uint<32> csr_offset = 0;
                    for (int pma_idx = 0; pma_idx < MAX_PMA_NUM; pma_idx++)
                    {
                        VEC_INFO vec_info = pos_pma_addrs[pma_idx];
                        if (vec_info == 0) break;
                        PMA_HEADER pma_header;
                        read_header(vec_info.range(31,0), hbm_ptr, pma_header);
                        for (int local_v = 0; local_v < VERTEX_PER_PMA; local_v++)
                        {
                            int src = pma_idx * VERTEX_PER_PMA + local_v;
                            // 输出row_ptr for this vertex
                            ap_axiu<32,0,0,0> row_ptr_pkt = {};
                            row_ptr_pkt.data = csr_offset;
                            row_ptr_pkt.keep = (ap_uint<8>)0xFF;
                            row_ptr_pkt.strb = (ap_uint<8>)0xFF;
                            row_ptr_pkt.last = 0;
                            data_to_csr_row_ptr_stream.write(row_ptr_pkt);
                            int emitted = read_edge(src, pma_header, vec_info.range(31,0), hbm_ptr, data_to_nei_query, data_to_coo_stream, data_to_csr_col_stream, 2);
                            csr_offset += emitted;
                        }
                    }
                    // 输出最后一个row_ptr[n]
                    ap_axiu<32,0,0,0> row_ptr_end = {};
                    row_ptr_end.data = csr_offset;
                    row_ptr_end.keep = (ap_uint<8>)0xFF;
                    row_ptr_end.strb = (ap_uint<8>)0xFF;
                    row_ptr_end.last = 1;
                    data_to_csr_row_ptr_stream.write(row_ptr_end);
                    data_to_csr_col_stream.write({.data=-1, .last=1});
                    break;
                }
                case PMA_GET_ALL_IN_EDGE_CSR:
                {
                    ap_uint<32> csr_offset = 0;
                    for (int pma_idx = 0; pma_idx < MAX_PMA_NUM; pma_idx++)
                    {
                        VEC_INFO vec_info = neg_pma_addrs[pma_idx];
                        if (vec_info == 0) break;
                        PMA_HEADER pma_header;
                        read_header(vec_info.range(31,0), hbm_ptr, pma_header);
                        for (int local_v = 0; local_v < VERTEX_PER_PMA; local_v++)
                        {
                            int src = pma_idx * VERTEX_PER_PMA + local_v;
                            // 输出row_ptr for this vertex
                            ap_axiu<32,0,0,0> row_ptr_pkt = {};
                            row_ptr_pkt.data = csr_offset;
                            row_ptr_pkt.keep = (ap_uint<8>)0xFF;
                            row_ptr_pkt.strb = (ap_uint<8>)0xFF;
                            row_ptr_pkt.last = 0;
                            data_to_csr_row_ptr_stream.write(row_ptr_pkt);
                            int emitted = read_edge(src, pma_header, vec_info.range(31,0), hbm_ptr, data_to_nei_query, data_to_coo_stream, data_to_csr_col_stream, 2);
                            csr_offset += emitted;
                        }
                    }
                    // 输出最后一个row_ptr[n]
                    ap_axiu<32,0,0,0> row_ptr_end = {};
                    row_ptr_end.data = csr_offset;
                    row_ptr_end.keep = (ap_uint<8>)0xFF;
                    row_ptr_end.strb = (ap_uint<8>)0xFF;
                    row_ptr_end.last = 1;
                    data_to_csr_row_ptr_stream.write(row_ptr_end);
                    data_to_csr_col_stream.write({.data=-1, .last=1});
                    break;
                }
                // case NONE:
                // {
                //     // vec_info_from_mem.read();
                //     // data_to_mem_stream.write({.data=0});
                //     break;
                // }
            }
        }
    }
}
