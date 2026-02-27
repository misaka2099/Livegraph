#include <cstdio>
#include <hls_stream.h>
#include "data_type.h"
#include "debug.h"
#include <ap_utils.h>
#define MAX_WORKERS 4
extern "C"
{
    void app_mem_middle_ware(
        hls::stream<COMMAND_STREAM> &cmd_from_app_1,
        hls::stream<VEC_INFO_STREAM> &vec_info_from_app_1,
        hls::stream<VEC_INFO_STREAM> &vec_info_to_app_1,

        hls::stream<COMMAND_STREAM> &cmd_from_app_2,
        hls::stream<VEC_INFO_STREAM> &vec_info_from_app_2,
        hls::stream<VEC_INFO_STREAM> &vec_info_to_app_2,

        
        hls::stream<COMMAND_STREAM> &cmd_from_app_3,
        hls::stream<VEC_INFO_STREAM> &vec_info_from_app_3,
        hls::stream<VEC_INFO_STREAM> &vec_info_to_app_3,

        hls::stream<COMMAND_STREAM> &cmd_from_app_4,
        hls::stream<VEC_INFO_STREAM> &vec_info_from_app_4,
        hls::stream<VEC_INFO_STREAM> &vec_info_to_app_4,

        hls::stream<VEC_INFO_STREAM> &vec_info_to_mem,
        hls::stream<COMMAND_STREAM> &cmd_to_mem,
        hls::stream<VEC_INFO_STREAM> &vec_info_from_mem,


        hls::stream<COMMAND_STREAM> &signal
    )
    {
        #pragma HLS INTERFACE axis port=cmd_from_app_1
        #pragma HLS INTERFACE axis port=vec_info_from_app_1
        #pragma HLS INTERFACE axis port=vec_info_to_app_1
        #pragma HLS INTERFACE axis port=cmd_from_app_2
        #pragma HLS INTERFACE axis port=vec_info_from_app_2
        #pragma HLS INTERFACE axis port=vec_info_to_app_2

        
        #pragma HLS INTERFACE axis port=cmd_from_app_3
        #pragma HLS INTERFACE axis port=vec_info_from_app_3
        #pragma HLS INTERFACE axis port=vec_info_to_app_3
        #pragma HLS INTERFACE axis port=cmd_from_app_4
        #pragma HLS INTERFACE axis port=vec_info_from_app_4
        #pragma HLS INTERFACE axis port=vec_info_to_app_4

        #pragma HLS INTERFACE axis port=vec_info_to_mem
        #pragma HLS INTERFACE axis port=cmd_to_mem
        #pragma HLS INTERFACE axis port=vec_info_from_mem
        #pragma HLS INTERFACE axis port=signal
        // bool alloc_app_1 = false;
        // bool alloc_app_2 = false;
        // bool free_app_1 = false;
        // bool free_app_2 = false;
        bool alloc_app[4];
        bool free_app[4];
        for (int i = 0 ; i < 4 ; i++)
        {
            alloc_app[i] = false;
            free_app[i] = false;
        }
        bool stopping = false;
        int next_worker_ptr = 0;
        while (true)
        {
            // 进入停止流程：不再接收新请求，只清空在途请求
            #pragma HLS PIPELINE II = 1
            if (!stopping && signal.empty() == false)
            {
                auto control_signal = signal.read();
                if (control_signal.data == MEM_STOP)
                    stopping = true;
                else if (control_signal.data == LOAD_BITMAP)
                {
                    vec_info_to_mem.write({.data = 0, .last = 0});
                    cmd_to_mem.write({.data = LOAD_BITMAP, .last = 0});
                    #pragma HLS protocol fixed
                    ap_wait();
                    vec_info_from_mem.read();  
                }
            }

            // 处理返回路径：无论是否 stopping 都要及时清空在途响应
            bool in_flight = alloc_app[0] || alloc_app[1] || alloc_app[2] || alloc_app[3] ||
                free_app[0] || free_app[1] || free_app[2] || free_app[3] ;
            if (in_flight)
            {
                VEC_INFO_STREAM info_back;
                bool is_back = vec_info_from_mem.read_nb(info_back);
                if (is_back)
                {
                    if (alloc_app[0])
                    {
                        // HERE
                        vec_info_to_app_1.write(info_back);
                        alloc_app[0] = false;
                    }
                    else if (alloc_app[1])
                    {
                        // HERE
                        vec_info_to_app_2.write(info_back);
                        alloc_app[1] = false;
                    }
                    else if (alloc_app[2])
                    {
                        // HERE
                        vec_info_to_app_3.write(info_back);
                        alloc_app[2] = false; // free 不回传给 app
                    }
                    else if (alloc_app[3])
                    {
                        // HERE
                        vec_info_to_app_4.write(info_back);
                        alloc_app[3] = false;
                    }
                    else if (free_app[0])
                    {
                        // HERE 
                        free_app[0] = false;
                    }
                    else if (free_app[1])
                    {
                        // HERE
                        free_app[1] = false;
                    }
                    else if (free_app[2])
                    {
                        // HERE 
                        free_app[2] = false;
                    }
                    else if (free_app[3])
                    {
                        // HERE 
                        free_app[3] = false;
                    }
                }
            }

            // 若处于 stopping，等待在途清空后发终止标记
            if (stopping)
            {
                // HERE
                if (!in_flight)
                {
                    // HERE
                    VEC_INFO_STREAM term; term.data = 0; term.last = 1;
                    vec_info_to_mem.write(term);
                    COMMAND_STREAM to_stop; to_stop.data = 0; to_stop.last = 1;
                    cmd_to_mem.write(to_stop);
                    break;
                }
                // 仍有在途，继续循环收敛
                continue;
            }

            bool process_in_this_cycle = false;

            switch (next_worker_ptr) 
            {
                case 0:
                {
                    process_in_this_cycle = cmd_from_app_1.empty() == false && vec_info_from_app_1.empty() == false && 
                            !in_flight;
                    break;
                }
                case 1:
                {
                    process_in_this_cycle = cmd_from_app_2.empty() == false && vec_info_from_app_2.empty() == false && 
                            !in_flight;
                    break;
                }
                case 2:
                {
                    process_in_this_cycle = cmd_from_app_3.empty() == false && vec_info_from_app_3.empty() == false && 
                            !in_flight;
                    break;
                }
                case 3:
                {
                    process_in_this_cycle = cmd_from_app_4.empty() == false && vec_info_from_app_4.empty() == false &&
                            !in_flight;
                    break;
                }
            }
            if (process_in_this_cycle)
            {
                switch (next_worker_ptr) 
                {
                    case 0:
                    {
                        VEC_INFO_STREAM info = vec_info_from_app_1.read();
                        COMMAND_STREAM cmd = cmd_from_app_1.read();
                        if (cmd.data == MEM_ALLOC)
                        {
                            // HERE
                            COMMAND_STREAM outc; outc.data = MEM_ALLOC; outc.last = 0;
                            vec_info_to_mem.write(info);
                            cmd_to_mem.write(outc);
                            alloc_app[0] = true;
                        }
                        else if (cmd.data == MEM_FREE)
                        {
                            // HERE
                            COMMAND_STREAM outc; outc.data = MEM_FREE; outc.last = 0;
                            vec_info_to_mem.write(info);
                            cmd_to_mem.write(outc);
                            free_app[0] = true;
                        }
                        break;
                    }
                    case 1:
                    {
                        VEC_INFO_STREAM info = vec_info_from_app_2.read();
                        COMMAND_STREAM cmd = cmd_from_app_2.read();
                        if (cmd.data == MEM_ALLOC)
                        {
                            // HERE
                            COMMAND_STREAM outc; outc.data = MEM_ALLOC; outc.last = 0;
                            vec_info_to_mem.write(info);
                            cmd_to_mem.write(outc);
                            alloc_app[1] = true;
                        }
                        else if (cmd.data == MEM_FREE)
                        {
                            // HERE
                            COMMAND_STREAM outc; outc.data = MEM_FREE; outc.last = 0;
                            vec_info_to_mem.write(info);
                            cmd_to_mem.write(outc);
                            free_app[1] = true;
                        }
                        break;
                    }
                    case 2:
                    {
                        VEC_INFO_STREAM info = vec_info_from_app_3.read();
                        COMMAND_STREAM cmd = cmd_from_app_3.read();
                        if (cmd.data == MEM_ALLOC)
                        {
                            // HERE
                            COMMAND_STREAM outc; outc.data = MEM_ALLOC; outc.last = 0;
                            vec_info_to_mem.write(info);
                            cmd_to_mem.write(outc);
                            alloc_app[2] = true;
                        }
                        else if (cmd.data == MEM_FREE)
                        {
                            // HERE
                            COMMAND_STREAM outc; outc.data = MEM_FREE; outc.last = 0;
                            vec_info_to_mem.write(info);
                            cmd_to_mem.write(outc);
                            free_app[2] = true;
                        }
                        break;
                    }
                    case 3:
                    {
                        VEC_INFO_STREAM info = vec_info_from_app_4.read();
                        COMMAND_STREAM cmd = cmd_from_app_4.read();
                        if (cmd.data == MEM_ALLOC)
                        {
                            // HERE
                            COMMAND_STREAM outc; outc.data = MEM_ALLOC; outc.last = 0;
                            vec_info_to_mem.write(info);
                            cmd_to_mem.write(outc);
                            alloc_app[3] = true;
                        }
                        else if (cmd.data == MEM_FREE)
                        {
                            // HERE
                            COMMAND_STREAM outc; outc.data = MEM_FREE; outc.last = 0;
                            vec_info_to_mem.write(info);
                            cmd_to_mem.write(outc);
                            free_app[3] = true;
                        }
                        break;
                    }
                }
            }
            next_worker_ptr  = (next_worker_ptr + 1) % MAX_WORKERS;
            // 正常接收新请求（仅在没有在途事务时）
            // if (cmd_from_app_1.empty() == false && vec_info_from_app_1.empty() == false && 
            //     alloc_app_1 == false && alloc_app_2 == false && free_app_1 == false && free_app_2 == false)
            // {
            //     VEC_INFO_STREAM info = vec_info_from_app_1.read();
            //     COMMAND_STREAM cmd = cmd_from_app_1.read();
            //     if (cmd.data == MEM_ALLOC)
            //     {
            //         HERE
            //         COMMAND_STREAM outc; outc.data = MEM_ALLOC; outc.last = 0;
            //         vec_info_to_mem.write(info);
            //         cmd_to_mem.write(outc);
            //         alloc_app_1 = true;
            //     }
            //     else if (cmd.data == MEM_FREE)
            //     {
            //         HERE
            //         COMMAND_STREAM outc; outc.data = MEM_FREE; outc.last = 0;
            //         vec_info_to_mem.write(info);
            //         cmd_to_mem.write(outc);
            //         free_app_1 = true;
            //     }
            // }
            // if (cmd_from_app_2.empty() == false && vec_info_from_app_2.empty() == false && 
            //    alloc_app_1 == false && alloc_app_2 == false && free_app_1 == false && free_app_2 == false)
            // {
            //     VEC_INFO_STREAM info = vec_info_from_app_2.read();
            //     COMMAND_STREAM cmd = cmd_from_app_2.read();
            //     if (cmd.data == MEM_ALLOC)
            //     {
            //         HERE
            //         COMMAND_STREAM outc; outc.data = MEM_ALLOC; outc.last = 0;
            //         vec_info_to_mem.write(info);
            //         cmd_to_mem.write(outc);
            //         alloc_app_2 = true;
            //     }
            //     else if (cmd.data == MEM_FREE)
            //     {
            //         HERE
            //         COMMAND_STREAM outc; outc.data = MEM_FREE; outc.last = 0;
            //         vec_info_to_mem.write(info);
            //         cmd_to_mem.write(outc);
            //         free_app_2 = true;
            //     }
            // }
        }
    }
}