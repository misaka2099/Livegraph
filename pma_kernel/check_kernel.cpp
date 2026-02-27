#include <cstdio>
#include <hls_stream.h>
#include <ap_int.h> 
#include "data_type.h"
#include "debug.h"
extern "C"
{
    void check_kernel(
        int *src,
        int *dst,
        double *vertex_attr,
        int *coo_src,
        int *coo_dst,
        int *row_ptr,
        int *col_idx,
        int *num_vertices,
        int *num_edges,
        int vertex_to_check,
        int attr_vertex_to_check,
        int mode,  // 0: check, 1: COO, 2: CSR
        int last,
        hls::stream<READ_COMMAND_STREAM> &cmd_to_read,
        hls::stream<ap_axiu<32,0,0,0>> &data_to_check,
        hls::stream<ap_axiu<64,0,0,0>> &attr_to_check,
        hls::stream<ap_axiu<64,0,0,0>> &data_to_coo_stream,
        hls::stream<ap_axiu<32,0,0,0>> &data_to_csr_row_ptr_stream,
        hls::stream<ap_axiu<32,0,0,0>> &data_to_csr_col_stream
    )
    {
        #pragma HLS INTERFACE m_axi port=src offset=slave bundle=gmem0
        #pragma HLS INTERFACE m_axi port=dst offset=slave bundle=gmem1
        #pragma HLS INTERFACE m_axi port=vertex_attr offset=slave bundle=gmem2
        #pragma HLS INTERFACE m_axi port=coo_src offset=slave bundle=gmem3
        #pragma HLS INTERFACE m_axi port=coo_dst offset=slave bundle=gmem4
        #pragma HLS INTERFACE m_axi port=row_ptr offset=slave bundle=gmem5
        #pragma HLS INTERFACE m_axi port=col_idx offset=slave bundle=gmem6
        #pragma HLS INTERFACE m_axi port=num_vertices offset=slave bundle=gmem7
        #pragma HLS INTERFACE m_axi port=num_edges offset=slave bundle=gmem8
        #pragma HLS INTERFACE axis port=cmd_to_read depth=16
        #pragma HLS INTERFACE axis port=data_to_check depth=16
        #pragma HLS INTERFACE axis port=attr_to_check depth=16
        #pragma HLS INTERFACE axis port=data_to_coo_stream depth=16
        #pragma HLS INTERFACE axis port=data_to_csr_row_ptr_stream depth=1024
        #pragma HLS INTERFACE axis port=data_to_csr_col_stream depth=1024
        #pragma HLS INTERFACE s_axilite port=src 
        #pragma HLS INTERFACE s_axilite port=dst 
        #pragma HLS INTERFACE s_axilite port=vertex_attr 
        #pragma HLS INTERFACE s_axilite port=coo_src 
        #pragma HLS INTERFACE s_axilite port=coo_dst 
        #pragma HLS INTERFACE s_axilite port=row_ptr 
        #pragma HLS INTERFACE s_axilite port=col_idx 
        #pragma HLS INTERFACE s_axilite port=num_vertices 
        #pragma HLS INTERFACE s_axilite port=num_edges 
        #pragma HLS INTERFACE s_axilite port=vertex_to_check 
        #pragma HLS INTERFACE s_axilite port=attr_vertex_to_check 
        #pragma HLS INTERFACE s_axilite port=mode 
        #pragma HLS INTERFACE s_axilite port=return 

        if (mode == 0) {  // check mode
            int idx = 0;
            for (int i = 0 ; i < vertex_to_check ; i++)
            {
                READ_COMMAND_STREAM cmd;
                cmd.data.range(31,0) = PMA_GET_OUT_EDGE;
                cmd.data.range(63,32) = src[i];
                cmd_to_read.write(cmd);

                int tmp_dst = 0;
                while (tmp_dst != -1)
                {
                    tmp_dst = data_to_check.read().data;
                    dst[idx] = tmp_dst;
                    idx++;
                }
            }
            for (int i = 0 ; i < attr_vertex_to_check ; i++)
            { 
                auto attr = attr_to_check.read();
                double attr_value = attr.data;
                vertex_attr[i] = attr_value;
            }
        } else if (mode == 1) {  // COO mode
            READ_COMMAND_STREAM cmd;
            cmd.data = PMA_GET_ALL_OUT_EDGE_COO;
            cmd.last = 0;
            cmd_to_read.write(cmd);

            int idx = 0;
            while (true)
            {
                ap_axiu<64,0,0,0> pkt = data_to_coo_stream.read();
                ap_uint<64> data = pkt.data;
                int src_val = (int)(data >> 32);
                int dst_val = (int)(data & 0xFFFFFFFF);
                if (pkt.last)
                {
                    break;
                }
                if (dst_val == -1)
                {
                    continue;
                }
                coo_src[idx] = src_val;
                coo_dst[idx] = dst_val;
                idx++;
            }
            *num_edges = idx;
        } else if (mode == 2) {  // CSR mode
            READ_COMMAND_STREAM cmd;
            cmd.data = PMA_GET_ALL_OUT_EDGE_CSR;
            cmd.last = 0;
            cmd_to_read.write(cmd);

            bool row_done = false;
            int v_idx = 0;
            int e_idx = 0;
            while (true)
            {
                if (!row_done)
                {
                    ap_axiu<32,0,0,0> pkt;
                    if (data_to_csr_row_ptr_stream.read_nb(pkt))
                    {
                        row_ptr[v_idx] = (int)pkt.data;
                        v_idx++;
                        if (pkt.last)
                        {
                            row_done = true;
                            *num_vertices = v_idx - 1;
                        }
                    }
                }

                ap_axiu<32,0,0,0> pkt_col;
                if (data_to_csr_col_stream.read_nb(pkt_col))
                {
                    if (pkt_col.last)
                    {
                        break;
                    }
                    col_idx[e_idx] = (int)pkt_col.data;
                    e_idx++;
                }
            }
            *num_edges = e_idx;
        }
        if (last)
            cmd_to_read.write({.data = PMA_STOP, .last = 1});
    }
}