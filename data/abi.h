#pragma once

#include "util/common.h"
class ClientBase;
class AbiBase {
public:
    virtual int init(ClientBase* client) = 0;
    virtual int get_data(ClientBase* client, uint64_t block_num, const std::vector<std::string>& pools) = 0;
    virtual int on_event(uint64_t pool_index, const json& json_data) = 0;
    virtual std::string get_logs_head() = 0;
    void set_index(uint32_t start, uint32_t size) {
        start_index = start;
        npools = size;
    }
    //virtual int update_data(ClientBase* client) = 0;
    uint32_t start_index;
    uint32_t npools;
};