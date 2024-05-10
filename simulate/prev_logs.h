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
        std::vector<LogEntry> logs;
        for (auto &[hash, pending_logs] : _logs) {
            logs.clear();
            request_tx_receipt(client, hash, logs);
            int cnt = 0;
            for (auto& log1 : logs) {
                for (auto& log2 : pending_logs) {
                    if (log1 == log2) {
                        ++cnt;
                        break;
                    }
                }
            }
            LOG(INFO) << "tx:" << hash << " logs_cnt:" << logs.size() << " match_cnt:" << cnt;
        }
    }
private:
    bthread_mutex_t _mutex;
    std::unordered_map<std::string, std::vector<LogEntry>> _logs;
};