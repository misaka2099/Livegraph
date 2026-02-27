#include "data_type.h"
#include "deprecated/xrt.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_bo.h>
#include <unordered_set>
#include <fstream>
#include <random>

void print_pma(
    int pma_idx,
    VEC_INFO* pos_info,
    BYTE* storage
)
{
    VEC_INFO vec_info = pos_info[pma_idx];
    int start = vec_info.range(31,0);
    int len =  vec_info.range(63,32);
    std::cout << "PMA " << pma_idx << " ADDR START FROM " << start << " LEN " << len << std::endl; 
    PMA_HEADER header;
    memcpy(&header, storage + start, sizeof(PMA_HEADER));
    int *pma_data = (int*)(storage + sizeof(PMA_HEADER) + start);
    for (int i = 0 ; i < 16 ; i++)
    {
        int src = pma_idx * 16  + i;
        int edge_count = header.edge_count[i];
        int seg_start = i == 0 ? 0 : header.vertex_range[i-1];
        int seg_end = header.vertex_range[i];
        std::cout << "vertex: " << src << " has " << edge_count << " neighbors" << std::endl;
        for (int j = seg_start ; j < seg_end ; j ++)
        {
            for (int k = 0 ; k < SEGMENT_SIZE ; k++)
            {
                std::cout <<pma_data[j*SEGMENT_SIZE + k] << " ";
            }
        }
        std::cout << std::endl;
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <xclbin> <graph>" << std::endl;
        return 1;
    }



    std::string xclbin_name = argv[1];
    std::string graph_path = argv[2];
    
    auto start_init = std::chrono::high_resolution_clock::now();
    
    std::fstream graph_file(graph_path);
    if (!graph_file.is_open()) {
        std::cerr << "Failed to open graph file: " << graph_path << std::endl;
        return 1;
    }
    struct Edge {
        int first, second;
        long long ts;
    };
    std::vector<std::vector<int>> Graph;
    int a,b;
    long long ts;
    std::vector<Edge> edges;
    int max_vertex = -1;
    //这一部分是在构建验证的答案
    while (graph_file >> a >> b >> ts)
    {
        edges.push_back({a, b, ts});
        if (a > max_vertex) max_vertex = a;
        if (b > max_vertex) max_vertex = b;
    }
    
    Graph.resize(max_vertex + 1);
    for (size_t i = 0; i < edges.size(); ++i) {
        Graph[edges[i].first].push_back(edges[i].second);
    }
    for (auto& neighbors : Graph) {
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    }
    int buffer_len = edges.size() + (max_vertex + 1);
    //
    graph_file.close();

    // 按时间戳排序，模拟真实动态图流
    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
        return a.ts < b.ts;
    });

    // 找到最小的时间戳
    long long min_ts = edges.empty() ? 0 : edges[0].ts;

    // 将所有具有最小时间戳的边作为 Base Graph，其余作为 Dynamic Stage
    size_t base_edges = 0;
    while (base_edges < edges.size() && edges[base_edges].ts == min_ts) {
        base_edges++;
    }
    
    // 如果所有边时间戳都一样，或者为了保证有增量阶段，可以至少留一些边（可选）
    // 这里严格按照你的要求：最小 timestamp 为 base，其他更新
    size_t incremental_edges = edges.size() - base_edges;


    // First, calculate max_vertex from base edges (static edges)
    int static_max_vertex = -1;
    for (size_t i = 0; i < base_edges; ++i) {
        if (edges[i].first > static_max_vertex) static_max_vertex = edges[i].first;
        if (edges[i].second > static_max_vertex) static_max_vertex = edges[i].second;
    }

    std::vector<std::vector<int>> BaseGraph(static_max_vertex + 1);
    for (size_t i = 0; i < base_edges; ++i) {
        BaseGraph[edges[i].first].push_back(edges[i].second);
    }
    for (auto& neighbors : BaseGraph) {
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    }

    // Debug: Print edges read from the file
    std::cout << "Edges read from the file:" << std::endl;

    // for (const auto& edge : edges) {
    //     std::cout << edge.first << " -> " << edge.second << std::endl;
    // }

    // // Debug: Print max_vertex value
    // std::cout << "Max vertex: " << max_vertex << std::endl;

    auto end_init = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_init = end_init - start_init;
    std::cout << "Initialization (File Read & Graph Build) time: " << elapsed_init.count() << " s" << std::endl;

    auto xclbin = xrt::xclbin(xclbin_name);
    auto device = xrt::device(0);
    auto uuid = device.load_xclbin(xclbin);

    // auto allocate_kernel = xrt::kernel(device, uuid, "allocate_kernel");
    // auto free_kernel = xrt::kernel(device, uuid, "free_kernel");
    // auto memalloc_kernel = xrt::kernel(device, uuid, "memalloc_kernel");
    auto alloc_free_kernel = xrt::kernel(device, uuid, "alloc_free_kernel");
    auto app_mem_middle_ware_kernel = xrt::kernel(device, uuid, "app_mem_middle_ware");
    auto pma_read_kernel = xrt::kernel(device, uuid, "pma_read_kernel");
    auto update_message_generator_kernel = xrt::kernel(device, uuid, "update_message_generator");
    auto update_message_router_kernel = xrt::kernel(device, uuid, "update_message_router");
    auto pma_insert_vertex_1 = xrt::kernel(device, uuid, "pma_insert_vertex:{pma_insert_vertex_1}");
    auto pma_insert_vertex_2 = xrt::kernel(device, uuid, "pma_insert_vertex:{pma_insert_vertex_2}");
    auto pma_insert_edge_1 = xrt::kernel(device, uuid, "pma_insert_edge:{pma_insert_edge_1}");
    auto pma_insert_edge_2 = xrt::kernel(device, uuid, "pma_insert_edge:{pma_insert_edge_2}");
    auto check_kernel = xrt::kernel(device, uuid, "check_kernel");
    auto signal_kernel = xrt::kernel(device, uuid, "signal_kernel");
    std::cout << "Kernels loaded successfully." << std::endl;
    // auto run_allocate_kernel = xrt::run(allocate_kernel);
    // auto run_free_kernel = xrt::run(free_kernel);
    auto run_alloc_free_kernel = xrt::run(alloc_free_kernel);
    auto run_app_mem_middle_ware_kernel = xrt::run(app_mem_middle_ware_kernel);
    // auto run_memalloc_kernel = xrt::run(memalloc_kernel);
    auto run_pma_read_kernel = xrt::run(pma_read_kernel);
    auto run_update_message_generator_kernel = xrt::run(update_message_generator_kernel);
    auto run_update_message_router_kernel = xrt::run(update_message_router_kernel);
    auto run_pma_insert_vertex_1 = xrt::run(pma_insert_vertex_1);
    auto run_pma_insert_vertex_2 = xrt::run(pma_insert_vertex_2);
    auto run_pma_insert_edge_1 = xrt::run(pma_insert_edge_1);
    auto run_pma_insert_edge_2 = xrt::run(pma_insert_edge_2);
    auto run_check_kernel = xrt::run(check_kernel);
    auto run_signal_kernel = xrt::run(signal_kernel);
    std::cout << "Kernels are created..." << std::endl;

    auto start_buffer = std::chrono::high_resolution_clock::now();

    auto bo_hbm_base_addr = xrt::bo(device, sizeof(BYTE) * (size_t)CHANNEL_CAPACITY * CHANNEL_NUM, pma_insert_vertex_1.group_id(2));
    // auto bo_valid_array = xrt::bo(device, sizeof(BYTE) * ((size_t)CHANNEL_CAPACITY * CHANNEL_NUM / 8), allocate_kernel.group_id(0));


    BYTE* hbm_base_address = bo_hbm_base_addr.map<BYTE*>();
    // BYTE* valid_array = bo_valid_array.map<BYTE*>();

    // 初始化 HBM 和有效性数组
    // std::fill_n(hbm_base_address, CHANNEL_CAPACITY * CHANNEL_NUM, -2);

    // std::fill_n(valid_array, CHANNEL_CAPACITY / 8, 0);

    auto bo_pos_vec_info = xrt::bo(device, sizeof(VEC_INFO) * MAX_PMA_NUM, pma_read_kernel.group_id(0));
    auto bo_neg_vec_info = xrt::bo(device, sizeof(VEC_INFO) * MAX_PMA_NUM, pma_read_kernel.group_id(1));
    auto pos_vec_info_ptr = bo_pos_vec_info.map<VEC_INFO*>();
    auto neg_vec_info_ptr = bo_neg_vec_info.map<VEC_INFO*>();
    std::fill_n(pos_vec_info_ptr, MAX_PMA_NUM, 0);
    std::fill_n(neg_vec_info_ptr, MAX_PMA_NUM, 0);

    // 修改 bo_src 和 bo_dst 的分配，确保使用 update_message_generator_kernel 的 group_id
    printf("edge_size %d\n", int(edges.size()));
    auto bo_src = xrt::bo(device, sizeof(int) * edges.size(), update_message_generator_kernel.group_id(2));
    auto bo_dst = xrt::bo(device, sizeof(int) * edges.size(), update_message_generator_kernel.group_id(3));

    int* src_ptr = bo_src.map<int*>();
    int* dst_ptr = bo_dst.map<int*>();

    // 边数据按阶段写入，初始不填充

    auto bo_check_src = xrt::bo(device, sizeof(int) * (max_vertex + 10), check_kernel.group_id(0));
    auto bo_check_dst = xrt::bo(device, sizeof(int) * buffer_len, check_kernel.group_id(1));
    auto check_src = bo_check_src.map<int*>();
    auto check_dst = bo_check_dst.map<int*>();
    auto bo_vertex_attr = xrt::bo(device, sizeof(double) *  (max_vertex + 10), check_kernel.group_id(2));
    auto bo_bitmap = xrt::bo(device, sizeof(BYTE) * ((size_t)CHANNEL_CAPACITY * CHANNEL_NUM / INIT_VEC_LEN_IN_BYTE / 8), alloc_free_kernel.group_id(3));
    auto bitmap_ptr = bo_bitmap.map<BYTE*>();
    std::fill_n(bitmap_ptr, (size_t)CHANNEL_CAPACITY * CHANNEL_NUM / 8 / INIT_VEC_LEN_IN_BYTE, 0);

    auto end_buffer = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_buffer = end_buffer - start_buffer;
    std::cout << "Buffer allocation and initialization time: " << elapsed_buffer.count() << " s" << std::endl;

    std::cout << "Buffers allocated and initialized." << std::endl;

    // run_allocate_kernel.set_arg(0, bo_valid_array);
    // run_free_kernel.set_arg(0, bo_valid_array);
    run_pma_read_kernel.set_arg(0, bo_pos_vec_info);
    run_pma_read_kernel.set_arg(1, bo_neg_vec_info);
    run_pma_read_kernel.set_arg(2, bo_hbm_base_addr);
    run_pma_insert_vertex_1.set_arg(0, bo_pos_vec_info);
    run_pma_insert_vertex_1.set_arg(1, bo_neg_vec_info);
    run_pma_insert_vertex_1.set_arg(2, bo_hbm_base_addr);
    run_pma_insert_vertex_2.set_arg(0, bo_pos_vec_info);
    run_pma_insert_vertex_2.set_arg(1, bo_neg_vec_info);
    run_pma_insert_vertex_2.set_arg(2, bo_hbm_base_addr);
    
    run_pma_insert_edge_1.set_arg(0, bo_pos_vec_info);
    run_pma_insert_edge_1.set_arg(1, bo_neg_vec_info);
    // run_pma_insert_edge_1.set_arg(2, bo_neg_vec_info);
    // run_pma_insert_edge_1.set_arg(3, bo_neg_vec_info);
    run_pma_insert_edge_1.set_arg(2, bo_hbm_base_addr);
    // run_pma_insert_edge_1.set_arg(5, bo_hbm_base_addr);
    // run_pma_insert_edge_1.set_arg(6, bo_hbm_base_addr);
    // run_pma_insert_edge_1.set_arg(7, bo_hbm_base_addr);
    // run_pma_insert_edge_1.set_arg(8, bo_hbm_base_addr);

    run_pma_insert_edge_2.set_arg(0, bo_pos_vec_info);
    run_pma_insert_edge_2.set_arg(1, bo_neg_vec_info );
    // run_pma_insert_edge_2.set_arg(2, bo_neg_vec_info);
    // run_pma_insert_edge_2.set_arg(3, bo_neg_vec_info);
    run_pma_insert_edge_2.set_arg(2, bo_hbm_base_addr);

    run_alloc_free_kernel.set_arg(3, bo_bitmap);
    // run_pma_insert_edge_2.set_arg(5, bo_hbm_base_addr);
    // run_pma_insert_edge_2.set_arg(6, bo_hbm_base_addr);
    // run_pma_insert_edge_2.set_arg(7, bo_hbm_base_addr);
    // run_pma_insert_edge_2.set_arg(8, bo_hbm_base_addr);

    auto verify_graph = [&](const std::vector<std::vector<int>>& expected_graph, int current_max_vertex, const std::string& label) {
        auto start_verify = std::chrono::high_resolution_clock::now();
        std::cout << "Verifying " << label << "..." << std::endl;
        std::vector<std::vector<int>> Updated_Graph(current_max_vertex + 1);
        
        for (int i = 0 ; i < current_max_vertex + 1; i++)
        {
            check_src[i] = i;
        }
        std::fill(check_dst, check_dst + buffer_len, -1);
        
        bo_check_src.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        bo_check_dst.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        
        run_check_kernel.set_arg(0, bo_check_src);
        run_check_kernel.set_arg(1, bo_check_dst);
        run_check_kernel.set_arg(2, bo_vertex_attr);
        run_check_kernel.set_arg(3, current_max_vertex + 1);
        run_check_kernel.set_arg(4, 0);
        
        run_pma_read_kernel.start();
        run_check_kernel.start();
        run_check_kernel.wait();
        run_pma_read_kernel.wait();
        
        bo_check_dst.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        std::cout << "Check kernel finished for " << label << "." << std::endl;
        
        int idx = 0;
        for (int i = 0; i < current_max_vertex + 1; i++) 
        {
            while (idx < buffer_len && check_dst[idx] != -1) {
                Updated_Graph[i].push_back(check_dst[idx]);
                idx++;
            }
            std::sort(Updated_Graph[i].begin(), Updated_Graph[i].end());
            Updated_Graph[i].erase(std::unique(Updated_Graph[i].begin(), Updated_Graph[i].end()), Updated_Graph[i].end());
            idx++; // Skip separator -1
        }
        
        std::string clean_label = label;
        std::replace(clean_label.begin(), clean_label.end(), ' ', '_');
        std::ofstream output_file("mismatch_" + clean_label + ".txt");
        
        bool all_match = true;
        for (int i = 0; i <= current_max_vertex; ++i) {
             const std::vector<int>& expected_neighbors = (i < expected_graph.size()) ? expected_graph[i] : std::vector<int>();
             
            if (expected_neighbors != Updated_Graph[i]) {
                output_file << "MISMATCH AT VERTEX " << i << std::endl;
                for (auto v : expected_neighbors)
                {
                    output_file << "EXPECTED EDGE: " << i << " -> " << v << std::endl;
                }
                for (auto v : Updated_Graph[i])
                {
                    output_file << "ACTUAL EDGE: " << i << " -> " << v << std::endl;
                }
                all_match = false;
            }
        }
        
        if (all_match) {
            std::cout << label << " matches perfectly!" << std::endl;
        } else {
            std::cout << label << " does not match. See mismatch_" << clean_label << ".txt for details." << std::endl;
        }
        auto end_verify = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_verify = end_verify - start_verify;
        std::cout << "Verification time for " << label << ": " << elapsed_verify.count() << " s" << std::endl;
    };

    auto add_base_graph_to_hbm = [&](int vertex_num, int edge_num) ->void
    {
        auto start_cpu_preprocess = std::chrono::high_resolution_clock::now();
        std::vector<ap_uint<64>> sorted_edges;
        std::vector<std::vector<int>> adj_pos(vertex_num+1); // src -> dst
        std::vector<std::vector<int>> adj_neg(vertex_num+1); // dst -> src

        for (size_t i = 0; i < edge_num; ++i) {
            int u = edges[i].first;
            int v = edges[i].second;
            if (u < vertex_num && v < vertex_num) {
                adj_pos[u].push_back(v);
                adj_neg[v].push_back(u);
            }
        }

        // 2. 对邻居进行排序
        for(auto& neighbors : adj_pos) std::sort(neighbors.begin(), neighbors.end());
        for(auto& neighbors : adj_neg) std::sort(neighbors.begin(), neighbors.end());

        int current_addr_offset = 0;
        // std::ofstream debug_pma("debug_pma_content.txt");

        // 3. 构建 PMA 结构
        for (int pma_idx = 0; pma_idx < (vertex_num + VERTEX_PER_PMA - 1) / VERTEX_PER_PMA; ++pma_idx) 
        {
            // POS Direction
            {
                PMA_HEADER pos_header = {0};
                std::vector<int> pos_data;
                
                for (int i = 0 ; i < VERTEX_PER_PMA; i++)
                {
                    int src = i + pma_idx * VERTEX_PER_PMA;
                    std::vector<int> neighbors;
                    if (src < vertex_num) {
                        neighbors = adj_pos[src];
                    }

                    pos_header.edge_count[i] = neighbors.size();
                    pos_header.attr[i] = 0;

                    int last_pos_range = i == 0 ? 0 : pos_header.vertex_range[i-1];
                    int total_segments = 1;
                    if (neighbors.size() > 0) {
                        total_segments = (neighbors.size() + SEGMENT_SIZE * DENSITY_BOUND - 1) / (SEGMENT_SIZE * DENSITY_BOUND);
                        int power_of_2 = 1;
                        while (power_of_2 < total_segments) {
                            power_of_2 *= 4;
                        }
                        total_segments = power_of_2;
                    }
                    pos_header.vertex_range[i] = last_pos_range + total_segments;

                    // Fill data
                    int base_num = 0;
                    int extra_num = 0;
                    if (total_segments > 0) {
                        base_num = neighbors.size() / total_segments;
                        extra_num = neighbors.size() % total_segments;
                    }
                    
                    int neighbor_idx = 0;
                    for (int s = 0; s < total_segments; ++s) {
                        int valid_num = base_num + (s < extra_num ? 1 : 0);
                        for (int k = 0; k < valid_num; ++k) {
                            pos_data.push_back(neighbors[neighbor_idx++]);
                        }
                        for (int k = valid_num; k < SEGMENT_SIZE; ++k) {
                            pos_data.push_back(UNUSED_ICON);
                        }
                    }
                }
                
                // Write to HBM
                memcpy(hbm_base_address + current_addr_offset, &pos_header, sizeof(PMA_HEADER));
                memcpy(hbm_base_address + current_addr_offset + sizeof(PMA_HEADER), pos_data.data(), pos_data.size() * sizeof(int));

                // debug_pma << "PMA " << pma_idx << " POS" << std::endl;
                // debug_pma << "START = " << current_addr_offset << " LEN = " << (sizeof(PMA_HEADER) + pos_data.size() * sizeof(int)) << std::endl;
                // for (int i = 0 ; i < 16 ; i++)
                // {
                //     debug_pma << "VERTEX_RANGE " << i << " = " << pos_header.vertex_range[i] << std::endl;
                // }
                // for (int i = 0 ; i < 16 ; i++)
                // {
                //     debug_pma << "EDGE COUNT " << i << " = " << pos_header.edge_count[i] << std::endl;
                // }
                // debug_pma << std::endl;
                
                // for (int i = 0 ; i < 16 ; i++)
                // {
                //     int start_seg = i == 0 ? 0 : pos_header.vertex_range[i-1];
                //     int end_seg = pos_header.vertex_range[i];
                //     debug_pma << "SRC " << i << " : ";
                //     for (int j = start_seg * SEGMENT_SIZE ; j < end_seg * SEGMENT_SIZE ; j++)
                //     {
                //         debug_pma << pos_data[j] << " ";
                //     }   
                //     debug_pma << std::endl;
                // }
                
                int total_len = sizeof(PMA_HEADER) + pos_data.size() * sizeof(int);
                int aligned_len = total_len;
                if (aligned_len % INIT_VEC_LEN_IN_BYTE != 0) aligned_len += (INIT_VEC_LEN_IN_BYTE - (aligned_len % INIT_VEC_LEN_IN_BYTE));
                pos_vec_info_ptr[pma_idx] = (ap_uint<64>(total_len) << 32) | current_addr_offset;
                
                current_addr_offset += aligned_len;
                // Align to 64 bytes
                // if (current_addr_offset % 64 != 0) current_addr_offset += (64 - (current_addr_offset % 64));
            }

            // NEG Direction
            {
                PMA_HEADER neg_header = {0};
                std::vector<int> neg_data;
                
                for (int i = 0 ; i < VERTEX_PER_PMA; i++)
                {
                    int src = i + pma_idx * VERTEX_PER_PMA;
                    std::vector<int> neighbors;
                    if (src < vertex_num) {
                        neighbors = adj_neg[src];
                    }

                    neg_header.edge_count[i] = neighbors.size();
                    neg_header.attr[i] = 0;

                    int last_pos_range = i == 0 ? 0 : neg_header.vertex_range[i-1];
                    int total_segments = 1;
                    if (neighbors.size() > 0) {
                        total_segments = (neighbors.size() + SEGMENT_SIZE * DENSITY_BOUND - 1) / (SEGMENT_SIZE * DENSITY_BOUND);
                        int power_of_2 = 1;
                        while (power_of_2 < total_segments) {
                            power_of_2 *= 4;
                        }
                        total_segments = power_of_2;
                    }
                    neg_header.vertex_range[i] = last_pos_range + total_segments;

                    // Fill data
                    int base_num = 0;
                    int extra_num = 0;
                    if (total_segments > 0) {
                        base_num = neighbors.size() / total_segments;
                        extra_num = neighbors.size() % total_segments;
                    }
                    
                    int neighbor_idx = 0;
                    for (int s = 0; s < total_segments; ++s) {
                        int valid_num = base_num + (s < extra_num ? 1 : 0);
                        for (int k = 0; k < valid_num; ++k) {
                            neg_data.push_back(neighbors[neighbor_idx++]);
                        }
                        for (int k = valid_num; k < SEGMENT_SIZE; ++k) {
                            neg_data.push_back(UNUSED_ICON);
                        }
                    }
                }
                
                // Write to HBM
                memcpy(hbm_base_address + current_addr_offset, &neg_header, sizeof(PMA_HEADER));
                memcpy(hbm_base_address + current_addr_offset + sizeof(PMA_HEADER), neg_data.data(), neg_data.size() * sizeof(int));

                // debug_pma << "PMA " << pma_idx << " NEG" << std::endl;
                // debug_pma << "START = " << current_addr_offset << " LEN = " << (sizeof(PMA_HEADER) + neg_data.size() * sizeof(int)) << std::endl;
                // for (int i = 0 ; i < 16 ; i++)
                // {
                //     debug_pma << "VERTEX_RANGE " << i << " = " << neg_header.vertex_range[i] << std::endl;
                // }
                // for (int i = 0 ; i < 16 ; i++)
                // {
                //     debug_pma << "EDGE COUNT " << i << " = " << neg_header.edge_count[i] << std::endl;
                // }
                // debug_pma << std::endl;
                
                // for (int i = 0 ; i < 16 ; i++)
                // {
                //     int start_seg = i == 0 ? 0 : neg_header.vertex_range[i-1];
                //     int end_seg = neg_header.vertex_range[i];
                //     debug_pma << "SRC " << i << " : ";
                //     for (int j = start_seg * SEGMENT_SIZE ; j < end_seg * SEGMENT_SIZE ; j++)
                //     {
                //         debug_pma << neg_data[j] << " ";
                //     }   
                //     debug_pma << std::endl;
                // }
                
                int total_len = sizeof(PMA_HEADER) + neg_data.size() * sizeof(int);
                int aligned_len = total_len;
                if (aligned_len % INIT_VEC_LEN_IN_BYTE != 0) aligned_len += (INIT_VEC_LEN_IN_BYTE - (aligned_len % INIT_VEC_LEN_IN_BYTE));
                neg_vec_info_ptr[pma_idx] = (ap_uint<64>(total_len) << 32) | current_addr_offset;
                
                current_addr_offset += aligned_len;
                // Align to 64 bytes
                // if (current_addr_offset % 64 != 0) current_addr_offset += (64 - (current_addr_offset % 64));
            }
        }
        // Update bitmap
        int total_blocks = (current_addr_offset + INIT_VEC_LEN_IN_BYTE - 1) / INIT_VEC_LEN_IN_BYTE;
        int full_bytes = total_blocks / 8;
        int remaining_bits = total_blocks % 8;
        memset(bitmap_ptr, 0xFF, full_bytes);
        
        if (remaining_bits > 0) {
            bitmap_ptr[full_bytes] |= (BYTE)((1 << remaining_bits) - 1);
        }
        // debug_pma.close();

        auto end_cpu_preprocess = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_cpu_preprocess = end_cpu_preprocess - start_cpu_preprocess;
        std::cout << "CPU preprocessing time in add_base_graph_to_hbm: " << elapsed_cpu_preprocess.count() << " s" << std::endl;

        bo_bitmap.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        std::cout << "base bitmap added to device." << std::endl;
        run_signal_kernel.set_arg(1, LOAD_BITMAP);
        run_signal_kernel.start();
        run_signal_kernel.wait();
    };

    // bo_valid_array.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    std::cout << "Buffers synchronized to device." << std::endl;

    std::cout << "Static vertex count: " << (static_max_vertex + 1) << std::endl;
    std::cout << "Static edge count: " << base_edges << std::endl;
    std::cout << "Incremental new vertex count: " << (max_vertex - static_max_vertex) << std::endl;
    std::cout << "Incremental edge count: " << incremental_edges << std::endl;
    std::cout << "Total max vertex after update: " << (max_vertex + 1) << std::endl;

    auto load_edges = [&](size_t start_idx, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            src_ptr[i] = edges[start_idx + i].first;
            dst_ptr[i] = edges[start_idx + i].second;
        }
        if (count > 0) {
            bo_src.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            bo_dst.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
    };

    auto run_phase = [&](size_t edge_start_idx, size_t edge_count, int new_vertex_count, const std::string& label) -> long long {
        if (edge_count == 0 && new_vertex_count == 0) {
            std::cout << label << ": vertices=0, edges=0, time=0 ms" << std::endl;
            return 0;
        }

        load_edges(edge_start_idx, edge_count);

        run_update_message_generator_kernel.set_arg(0, new_vertex_count);
        run_update_message_generator_kernel.set_arg(1, static_cast<int>(edge_count));
        run_update_message_generator_kernel.set_arg(2, bo_src);
        run_update_message_generator_kernel.set_arg(3, bo_dst);

        auto phase_start = std::chrono::high_resolution_clock::now();

        run_pma_insert_vertex_1.start();
        run_pma_insert_vertex_2.start();
        run_pma_insert_edge_1.start();
        run_pma_insert_edge_2.start();
        run_update_message_router_kernel.start();
        run_update_message_generator_kernel.start();

        run_update_message_generator_kernel.wait();
        run_update_message_router_kernel.wait();
        run_pma_insert_edge_1.wait();
        run_pma_insert_edge_2.wait();
        run_pma_insert_vertex_1.wait();
        run_pma_insert_vertex_2.wait();

        auto phase_end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(phase_end - phase_start).count();
        std::cout << label << ": vertices=" << new_vertex_count << ", edges=" << edge_count
                  << ", time=" << elapsed << " ms" << std::endl;
        return elapsed;
    };


    run_alloc_free_kernel.start();
    run_app_mem_middle_ware_kernel.start();
    // run_memalloc_kernel.start();
    
    add_base_graph_to_hbm(static_max_vertex + 1, base_edges);
    std::cout << "Base graph loaded into HBM." << std::endl;
    bo_pos_vec_info.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_neg_vec_info.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_src.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_dst.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_hbm_base_addr.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_bitmap.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    // verify_graph(BaseGraph, static_max_vertex, "Base Graph");

    // long long base_phase_ms = run_phase(0, base_edges, static_max_vertex + 1, "Base graph build");
    std::cout << "\n--- Starting Incremental Updates ---" << std::endl;
    long long base_phase_ms = 0;
    long long incremental_phase_ms = 0;
    int current_max_v = static_max_vertex;

    struct PhaseResult {
        long long timestamp;
        size_t edge_count;
        int new_vertices;
        long long duration_ms;
    };
    std::vector<PhaseResult> phase_results;

    size_t i = base_edges;
    while (i < edges.size()) {
        size_t j = i;
        long long current_ts = edges[i].ts;
        int batch_max_v = -1;

        // 找到所有具有相同时间戳的边，作为一个批次
        while (j < edges.size() && edges[j].ts == current_ts) {
            if (edges[j].first > batch_max_v) batch_max_v = edges[j].first;
            if (edges[j].second > batch_max_v) batch_max_v = edges[j].second;
            j++;
        }

        size_t current_batch_count = j - i;
        
        // 计算当前批次新增的顶点数量
        int new_v_count = (batch_max_v > current_max_v) ? (batch_max_v - current_max_v) : 0;
        
        std::string label = "Timestamp Update [" + std::to_string(current_ts) + "]";
        long long duration = run_phase(i, current_batch_count, new_v_count, label);
        
        phase_results.push_back({current_ts, current_batch_count, new_v_count, duration});
        incremental_phase_ms += duration;
        
        // 更新当前已知的最大顶点序号
        if (batch_max_v > current_max_v) {
            current_max_v = batch_max_v;
        }
        
        // 移动到下一个时间戳的起始位置
        i = j;
    }

    std::cout << "\n--- Detailed Phase Results ---" << std::endl;
    std::cout << "Timestamp\tEdges\tNew_Verts\tTime(ms)" << std::endl;
    for (const auto& res : phase_results) {
        std::cout << res.timestamp << "\t" << res.edge_count << "\t" 
                  << res.new_vertices << "\t\t" << res.duration_ms << std::endl;
    }
    std::cout << "------------------------------\n" << std::endl;

    std::cout << "GRAPH UPDATED. total_time=" << (base_phase_ms + incremental_phase_ms)
              << " ms" << std::endl;

    verify_graph(Graph, max_vertex, "Full Graph");
    // run_stop_kernel.start();
    // run_stop_kernel.wait();
    run_signal_kernel.set_arg(1, MEM_STOP);
    run_signal_kernel.start();
    run_signal_kernel.wait();
    std::cout << "Stop signal sent to memory kernel." <<  std::endl;
    std::cout << "update edges " << edges.size() << " base_time " << base_phase_ms
              << " ms incremental_time " << incremental_phase_ms << " ms total "
              << (base_phase_ms + incremental_phase_ms) << " ms" << std::endl;

    // run_allocate_kernel.wait();
    // run_free_kernel.wait();
    run_alloc_free_kernel.wait();
    run_app_mem_middle_ware_kernel.wait();
    // run_memalloc_kernel.wait();
    // bo_hbm_base_addr.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    // bo_pos_vec_info.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    // bo_neg_vec_info.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    // BYTE* storage = bo_hbm_base_addr.map<BYTE*>();
    // auto pos_info = bo_pos_vec_info.map<VEC_INFO*>();
    // auto neg_info = bo_neg_vec_info.map<VEC_INFO*>();
    // std::cout << "----------------POS_DIRECTION-----------------" << std::endl;
    // for (int i = 0 ; i < (max_vertex + VERTEX_PER_PMA - 1)/ VERTEX_PER_PMA ; i++)
    // {
    //     print_pma(i, pos_info, storage);
    // }
    // std::cout << "----------------NEG_DIRECTION-----------------" <<std::endl;
    //     for (int i = 0 ; i < (max_vertex + VERTEX_PER_PMA - 1)/ VERTEX_PER_PMA ; i++)
    // {
    //     print_pma(i, neg_info, storage);
    // }
    return 0;
}