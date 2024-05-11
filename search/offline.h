/*#pragma once
#include "util/common.h"
#include "search/search_result.h"
class OfflineSearch : public Singleton<OfflineSearch> {
public:
    void start(uint32_t thread_num, uint32_t limit_length);
    void search();
    void compute(const std::vector<uint32_t>& path, const std::vector<bool>& direction);
private:
    void dfs(uint32_t start_token, butil::FlatSet<uint32_t>& visited_set, uint32_t cur_token, uint32_t len, std::vector<uint32_t>& path, std::vector<bool>& directioin);
    uint32_t _limit_length;
    std::atomic<uint32_t> _cur_token;
};
*/