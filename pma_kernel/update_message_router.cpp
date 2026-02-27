#include "data_type.h"
#include "debug.h"
#include "debug.h"
#include "hls_stream.h"
#include <cstdio>
// 可配置的 in-flight 上限（可在编译命令中定义以覆盖默认值）
#ifndef MAX_INFLIGHT_NUM
#define MAX_INFLIGHT_NUM 16
#endif
// static int task_tag = 0;
void flush_buffer_aux(
    int buffer_idx,
    hls::stream<PMA_UPDATE_MESSAGE> &cmd_to_pma,
    ap_uint<128> msg_buffer[MAX_COMMAND_BUFFER_NUM][MAX_COMMAND_BUFFER_LENGTH],
    int *count_arr
)
{
    int c = count_arr[buffer_idx];
    for (int j = 0 ; j < c; j++) {
        #pragma HLS PIPELINE II=1
        PMA_UPDATE_MESSAGE tmp;
        tmp.data = msg_buffer[buffer_idx][j];
        tmp.last = (j == c - 1) ? 1 : 0;
        cmd_to_pma.write(tmp);
        printf("Flushing buffer %d, command type: %d, SRC = %d, DST = %d\n", buffer_idx, int(tmp.data.range(97,96)), 
        int(tmp.data.range(31,0)), int(tmp.data.range(63,32)));
    }
    count_arr[buffer_idx] = 0;
}

bool check_inflight(
    int *in_flight_arr,
    int pma_idx
)
{
    for (int i = 0 ; i < MAX_INFLIGHT_NUM ; i++)
    {
        if (in_flight_arr[i] == pma_idx)
        {
            return true;
        }
    }
    return false;
}

void check_ack_back(
    int *in_flight_arr,
    int &in_flight_count,
    hls::stream<COMMAND_STREAM>& ack_from_insert_edge_1,
    hls::stream<COMMAND_STREAM>& ack_from_insert_edge_2
)
{
    COMMAND_STREAM ack;
    if (ack_from_insert_edge_1.read_nb(ack))
    {
        int pma_idx = ack.data;
        for (int i = 0 ; i < MAX_INFLIGHT_NUM ; i++)
        {
            if (in_flight_arr[i] == pma_idx)
            {
                in_flight_arr[i] = -1;
                in_flight_count--;
                break;
            }
        }
    }
    if (ack_from_insert_edge_2.read_nb(ack))
    {
        int pma_idx = ack.data;
        for (int i = 0 ; i < MAX_INFLIGHT_NUM ; i++)
        {
            if (in_flight_arr[i] == pma_idx)
            {
                in_flight_arr[i] = -1;
                in_flight_count--;
                break;
            }
        }
    }
}
void add_inflight(
    int *in_flight_arr,
    int &in_flight_count,
    int pma_idx
)
{
    for (int i  = 0 ; i < MAX_INFLIGHT_NUM ; i++)
    {
        if (in_flight_arr[i] == -1)
        {
            in_flight_arr[i] = pma_idx;
            in_flight_count++;
            break;
        }
    }
}


void flush_and_add_inflight(
    int hash,
    int pma_idx,
    int *in_flight_arr,
    int &in_flight_count,
    hls::stream<COMMAND_STREAM>& ack_from_insert_edge_1,
    hls::stream<COMMAND_STREAM>& ack_from_insert_edge_2,
    hls::stream<PMA_UPDATE_MESSAGE> &cmd_to_insert_edge_1,
    hls::stream<PMA_UPDATE_MESSAGE> &cmd_to_insert_edge_2,
    ap_uint<128> msg_buffer[MAX_COMMAND_BUFFER_NUM][MAX_COMMAND_BUFFER_LENGTH],
    int *count_arr
)
{
    // 等待没有冲突且 inflight 未满
    while (check_inflight(in_flight_arr, pma_idx) || in_flight_count >= MAX_INFLIGHT_NUM)
    {
        check_ack_back(
            in_flight_arr,
            in_flight_count,
            ack_from_insert_edge_1, 
            ack_from_insert_edge_2
        );
    }
    // 只有 buffer 有数据时才 flush
    if (count_arr[hash] > 0) {
        if (hash % 2 == 0) {
            flush_buffer_aux(hash, cmd_to_insert_edge_1, msg_buffer, count_arr);
        } else {
            flush_buffer_aux(hash, cmd_to_insert_edge_2, msg_buffer, count_arr);
        }
        add_inflight(in_flight_arr, in_flight_count, pma_idx);
    }
}


extern "C"
{
    void update_message_router(
        hls::stream<PMA_UPDATE_MESSAGE> &cmd_from_generator,
        hls::stream<PMA_UPDATE_MESSAGE> &cmd_to_insert_vertex_1,
        hls::stream<PMA_UPDATE_MESSAGE> &cmd_to_insert_vertex_2,
        hls::stream<PMA_UPDATE_MESSAGE> &cmd_to_insert_edge_1,
        hls::stream<PMA_UPDATE_MESSAGE> &cmd_to_insert_edge_2,
        hls::stream<COMMAND_STREAM> &ack_from_insert_edge_1,
        hls::stream<COMMAND_STREAM> &ack_from_insert_edge_2,
        hls::stream<COMMAND_STREAM> &ack_from_insert_vertex_1,
        hls::stream<COMMAND_STREAM> &ack_from_insert_vertex_2,
        hls::stream<COMMAND_STREAM> &ack_to_generator
        // hls::stream<VERTEX_ATTR_STREAM_TYPE> &attr_from_generator,
        // hls::stream<VERTEX_ATTR_STREAM_TYPE> &attr_to_pma,
        // hls::stream<PMA_UPDATE_MESSAGE> &cmd_to_attr_updater,
        // hls::stream<COMMAND_STREAM> &ack_from_attr_updater
        // hls::stream<CLEAR_ICON> &clear_message_from_generator
    )
    {
        #pragma HLS INTERFACE axis port=cmd_from_generator
        #pragma HLS INTERFACE axis port=cmd_to_insert_vertex_1
        #pragma HLS INTERFACE axis port=cmd_to_insert_vertex_2
        #pragma HLS INTERFACE axis port=cmd_to_insert_edge_1
        #pragma HLS INTERFACE axis port=cmd_to_insert_edge_2
        #pragma HLS INTERFACE axis port=ack_from_insert_edge_1
        #pragma HLS INTERFACE axis port=ack_from_insert_edge_2
        #pragma HLS INTERFACE axis port=ack_from_insert_vertex_1
        #pragma HLS INTERFACE axis port=ack_from_insert_vertex_2
        #pragma HLS INTERFACE axis port=ack_to_generator
        // #pragma HLS INTERFACE axis port=attr_from_generator
        // #pragma HLS INTERFACE axis port=attr_to_pma
        // #pragma HLS INTERFACE axis port=cmd_to_attr_updater
        // #pragma HLS INTERFACE axis port=ack_from_attr_updater
        // #pragma HLS INTERFACE axis port=clear_message_from_generator
        // int processing_pma_idx[MAX_WORKERS] = {-1};
        // #pragma HLS ARRAY_PARTITION variable=processing_pma_idx complete
        // for (int i = 0 ; i < MAX_WORKERS ; i++)
        // {
        //     #pragma HLS UNROLL
        //     processing_pma_idx[i] = -1;
        // }
        ap_uint<128> msg_buffer[MAX_COMMAND_BUFFER_NUM][MAX_COMMAND_BUFFER_LENGTH];
        #pragma HLS BIND_STORAGE variable=msg_buffer type=RAM_2P impl=URAM
        int count[MAX_COMMAND_BUFFER_NUM]; 
        #pragma HLS ARRAY_PARTITION variable=count complete
        int in_flight[MAX_INFLIGHT_NUM];
        for (int i = 0 ; i < MAX_INFLIGHT_NUM ; i++)
        {
            in_flight[i] = -1;
        }
        int in_flight_count = 0;
        for (int i = 0 ; i < MAX_COMMAND_BUFFER_NUM ; i++)
        {
            // #pragma HLS UNROLL
            for (int j = 0 ; j < MAX_COMMAND_BUFFER_LENGTH ; j++)
            {
                #pragma HLS UNROLL
                msg_buffer[i][j] = 0;
            }
            count[i] = 0;
        }
        int stage = 0;
        PMA_UPDATE_MESSAGE msg;
        bool quit = false;
        while (true)
        {
            switch (stage)
            {
                case 0://insert vertex
                {
                    if(cmd_from_generator.read_nb(msg))
                    {
                        if (msg.data == -1)
                        {
                            HERE
                            stage++;
                            msg.data = PMA_STOP;
                            // msg.last = 1;
                            cmd_to_insert_vertex_1.write(msg);
                            cmd_to_insert_vertex_2.write(msg);
                            break;
                        }
                        int src = msg.data.range(31,0);  
                        int pma_idx = src / VERTEX_PER_PMA;
                        if (pma_idx % 2 == 0) {
                            cmd_to_insert_vertex_1.write(msg);
                        } else {
                            cmd_to_insert_vertex_2.write(msg);
                        }
                    }
                    break;
                }
                case 1:
                {
                    bool ack1 = false;
                    bool ack2 = false;
                    while (!(ack1 && ack2))
                    {
                        COMMAND_STREAM tmp;
                        if (ack_from_insert_vertex_1.read_nb(tmp))
                        {
                            ack1 = true;
                        }
                        if (ack_from_insert_vertex_2.read_nb(tmp))
                        {
                            ack2 = true;
                        }
                    }
                    stage++;
                }
                case 2:
                {
                    check_ack_back(
                        in_flight,
                        in_flight_count,
                        ack_from_insert_edge_1,
                        ack_from_insert_edge_2
                    );
                    if(cmd_from_generator.read_nb(msg))
                    {
                        HERE
                        if (msg.data == -1)
                        {
                            HERE
                            // stage++;
                            msg.data = PMA_STOP;
                            // msg.last = 1;
                            for (int i = 0 ; i < MAX_COMMAND_BUFFER_NUM ; i++)
                            {
                                if (count[i] > 0)
                                flush_and_add_inflight(
                                    i, msg_buffer[i][0].range(31,0) / VERTEX_PER_PMA,
                                    in_flight, in_flight_count,
                                    ack_from_insert_edge_1, ack_from_insert_edge_2,
                                    cmd_to_insert_edge_1, cmd_to_insert_edge_2,
                                    msg_buffer, count
                                );
                            }
                            if (msg.last == 1)
                            {
                                cmd_to_insert_edge_1.write(msg);
                                cmd_to_insert_edge_2.write(msg);
                                quit = true;
                                stage++;
                            }
                            else  
                            {
                                stage = 0;
                            }
                            while (in_flight_count > 0)
                            {
                                check_ack_back(
                                    in_flight,
                                    in_flight_count,
                                    ack_from_insert_edge_1,
                                    ack_from_insert_edge_2
                                );
                            }
                            // If this batch is the final (last==1), send an ACK back to the generator to notify completion
                            COMMAND_STREAM ack_pkt;
                            ack_pkt.data = 1;
                            ack_pkt.keep = (ap_uint<4>)0xF;
                            ack_pkt.strb = (ap_uint<4>)0xF;
                            ack_pkt.last = 1;
                            ack_to_generator.write(ack_pkt);
                            break;
                        }
                        int src = msg.data.range(31,0);  
                        int pma_idx = src / VERTEX_PER_PMA;
                        int hash = pma_idx % (MAX_COMMAND_BUFFER_NUM / 2);
                        if (msg.data.range(98,98) == OUT_DIRECTION)
                        {
                            hash += MAX_COMMAND_BUFFER_NUM / 2;
                        }
                        if (count[hash] == 0)
                        {
                            HERE
                            msg_buffer[hash][count[hash]] = msg.data;
                            count[hash]++;
                        }
                        else if (count[hash] < 16)
                        {
                            if (msg_buffer[hash][0].range(31,0) / VERTEX_PER_PMA == msg.data.range(31,0) / VERTEX_PER_PMA)
                            {
                                // HERE
                                msg_buffer[hash][count[hash]] = msg.data;
                                count[hash]++;
                            }
                            else  
                            {
                                HERE
                                flush_and_add_inflight(
                                    hash, msg_buffer[hash][0].range(31,0) / VERTEX_PER_PMA,
                                    in_flight, in_flight_count,
                                    ack_from_insert_edge_1, ack_from_insert_edge_2,
                                    cmd_to_insert_edge_1, cmd_to_insert_edge_2,
                                    msg_buffer, count
                                );
                                msg_buffer[hash][count[hash]] = msg.data;
                                count[hash]++;
                            }
                        }
                        else  
                        {
                            HERE
                            flush_and_add_inflight(
                                hash, msg_buffer[hash][0].range(31,0) / VERTEX_PER_PMA,
                                in_flight, in_flight_count,
                                ack_from_insert_edge_1, ack_from_insert_edge_2,
                                cmd_to_insert_edge_1, cmd_to_insert_edge_2,
                                msg_buffer, count
                            );
                            msg_buffer[hash][count[hash]] = msg.data;
                            count[hash]++;
                            
                        }
                    }
                    break;
                }
                case 3:
                {
                    quit = true;
                    break;
                }
            }
            if (quit)
            {
                break;
            }
        }
    }
}