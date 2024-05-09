#include "data/client.h"
#include <brpc/channel.h>

class IpcClient : public ClientBase {
public:
    int connect(const std::string &address);
    
    int read(json &buffer) override;

    int _write(const json &in) override;
private:
};