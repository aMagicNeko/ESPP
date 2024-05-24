#include "util/common.h"
#include "util/type.h"
#include "unordered_map"
#include "data/request.h"
class PrevLogs : public Singleton<PrevLogs> {
public:
    PrevLogs() {
        bthread_mutex_init(&_mutex, 0);
    }
    void add_log(std::string hash, const LogEntry& log) {
        LockGuard lock(&_mutex);
        if (!_logs.count(hash)) {
            _logs.emplace(hash, std::vector<LogEntry>());
        }
        _logs[hash].push_back(log);
    }
    void on_head(ClientBase* client) {
        LockGuard lock(&_mutex);
        std::vector<LogEntry> logs;
        // TODO: analyze logs not match situation
        for (auto &[hash, pending_logs] : _logs) {
            logs.clear();
            request_tx_receipt(client, hash, logs);
            int cnt = 0;
            //LOG(INFO) << "simulate logs:";
            //for (auto& log : pending_logs) {
            //    LOG(INFO) << log.to_string();
            //}
            //LOG(INFO) << "real logs:";
            //for (auto& log : logs) {
            //    LOG(INFO) << log.to_string();
            //}
            for (auto& log1 : logs) {
                for (auto& log2 : pending_logs) {
                    if (log1 == log2) {
                        ++cnt;
                        break;
                    }
                    if (log1.address == log2.address) {
                        LOG(INFO) << "match log address, but not topic";
                        LOG(INFO) << log1.to_string();
                        LOG(INFO) << log2.to_string();
                    }
                }
            }
            LOG(INFO) << "tx:" << hash << " logs_cnt:" << logs.size() << " match_cnt:" << cnt;
        }
        _logs.clear();
    }
private:
    bthread_mutex_t _mutex;
    std::unordered_map<std::string, std::vector<LogEntry>> _logs;
};