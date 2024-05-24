#include "search/online.h"
DEFINE_string(host, "45.250.254.213", "RPC host");
DEFINE_string(port, "8546", "RPC port");
DEFINE_string(path, "/", "RPC path");
DEFINE_int32(ssl, 0, "is SSL");
DEFINE_string(ipc_url, "", "url of ipc socket");
SearchResult compute_impl(std::vector<PoolBase*> path, std::vector<bool> direction);
#include "search/analytical_solution.h"
// 示例：

std::vector<int256_t> parse_log(const std::string& s) {
    std::stringstream ss;
    ss << s;
    int256_t x;
    std::vector<int256_t> res;
    while (ss >> x) {
        res.push_back(x);
    }
    return res;
}
int main() {
    
    UniswapV3Pool pool(1, 56, Address("0x075e94d6115373973ba34989cfa1754f89bd32e3"), 10000, 200);
    pool.tick = -195915;
    pool.liquidity = uint128_t("272589725297405082");
    pool.sqrt_price = uint128_t("4414305711559188345406485");
    auto liqs = parse_log("-196352 115792089237316195423570985008687907853269984665640564039457584007913129639935 -196224 1010415561354318 -196094 -78195380914566 -196059 -24448431016597321 -196026 520202268528394 -195833 729663811259263 -195666 -106925946128513 -195611 -371549156461246 -195587 -4293502247362936 -195585 -115792089237316195423570985008687907853269984665640564039457584007913129639935");
    for (int i = 0; i < liqs.size(); i += 2) {
        pool.liquidity_net[liqs[i].convert_to<int>()] = liqs[i+1];
    }
    
    UniswapV3Pool pool2(0, 56, Address("0xf6579569b34c675f3d2c17d30fdca134eec0fe53"), 100, 1);
    pool2.tick = -1;
    pool2.liquidity = uint128_t("141245734580690848");
    pool2.sqrt_price = uint128_t("79226536904713775378411144107");
    liqs = parse_log("-512 115792089237316195423570985008687907853269984665640564039457584007913129639935 -296 38647527064 -102 204004711968 -100 89179463 -98 7691293177 -77 2009741 -52 3934979497517 -50 2996378655406 -49 392989184 -44 44108387655 -41 6695618837 -38 3494709585741 -36 21329472506 -34 213025742309305 -31 5876297144 -29 3713267354 -27 512682451 -25 362020916610654 -24 15870576204876 -23 7278047924526 -22 22046342106058 -21 51718224888376 -20 854368935069 -19 22923725257320 -18 82137294128 -17 90092892355511 -16 113198968446301 -15 1239845746374562 -14 165926401125761 -13 1428967282406981 -12 821465165294669 -11 1979348094433977 -10 1284400178647950 -9 99599574297857 -8 443902004013629 -7 1384380041497249 -6 3221258784802320 -5 1958553414648110 -4 721168859695328 -3 6785305330653117 -2 235720634589226 -1 118566014045749446 0 -115886645989144541 1 -1387782015167347 2 -1899096587329146 3 -6181296413232774 4 -662922402163989 5 -2325446719357062 6 -349976409348186 7 -5472210175038452 8 -1152443444707111 9 -2149539964741802 10 -222769320034990 11 -1895191950681028 12 -307811064686397 13 -764110320221 14 -198780544927852 15 -15370858395940 16 -16261059919475 17 -7489296068936 18 -22358209464990 19 -18757493482151 20 -183126852456426 21 -55073833528 22 -129221995290075 23 -32980683629740 24 -413023590605547 25 -213568437565034 26 -27269027603 29 -44644095397060 30 -51129164047330 33 -797024325197 37 -44108387655 49 -3934979497517 52 -392989184 58 -39450004 95 -20032 99 -204004711968 100 -7780472640 255 -115792089237316195423570985008687907853269984665640564039457584007913129639935");
    for (int i = 0; i < liqs.size(); i += 2) {
        pool2.liquidity_net[liqs[i].convert_to<int>()] = liqs[i+1];
    }
   
    UniswapV3Pool pool3(154707, 0, Address("0x70cf99553471fe6c0d513ebfac8acc55ba02ab7b"), 3000, 60);
    pool3.tick = -392461;
    pool3.liquidity = uint128_t("58510854162026782994");
    pool3.sqrt_price = uint128_t("238311524549277392713");
    liqs = parse_log("-418816 340282366920938463463374607431768211455 -372796 -340282366920938463463374607431768211455");
    for (int i = 0; i < liqs.size(); i += 2) {
        pool2.liquidity_net[liqs[i].convert_to<int>()] = liqs[i+1];
    }

    UniswapV2Pool pool4(154707, 1, Address("0x198063c23ac1317ff3cc57d9f54faac6b675d89f"), uint256_t("2193003881274539070922426978897"), uint256_t("6640462745124196013617"));
    std::vector<std::shared_ptr<PoolBase>> path;
    std::vector<bool> direction;
    direction.push_back(1);
    direction.push_back(0);
    direction.push_back(0);
    direction.push_back(1);
    path.push_back(std::shared_ptr<PoolBase>(&pool));
    path.push_back(std::shared_ptr<PoolBase>(&pool2));
    path.push_back(std::shared_ptr<PoolBase>(&pool3));
    path.push_back(std::shared_ptr<PoolBase>(&pool4));

    uint256_t accumulate_input = 0;
    uint256_t accumulate_output = 0;
    uint256_t max = 0;
    uint256_t res_in = 0;
    LOG(DEBUG) << "-------------compute start";
    for (uint32_t i = 0; i < path.size(); ++i) {
        LOG(DEBUG) << path[i]->to_string();
    }
    int nrounds = 0;
    while (nrounds++ < 100) {
        LOG(DEBUG) << "------round start";
        for (auto p:path) {
            LOG(INFO) << p->to_string();
        }
        uint256_t token_in = get_analytical_solution(path, direction);
        LOG(DEBUG) << "analytical_solution out:" << token_in;
        if (token_in == 0) {
            break;
        }
        uint256_t cur_boundary = MAX_TOKEN_NUM - 1;
        LOG(INFO) << "cur boundary:" << cur_boundary;
        for (uint32_t i = 0; i < path.size(); ++i) {
            cur_boundary = path[i]->get_output_boundary(cur_boundary, direction[i]);
            if (cur_boundary == 0) {
                // end
                break;
            }
            LOG(INFO) << "cur boundary:" << cur_boundary;
        }
        LOG(INFO) <<  cur_boundary;
        if (cur_boundary == 0) {
            // end
            break;
        }
        for (int i = path.size() - 1; i >= 0; --i) {
            cur_boundary = path[i]->compute_input(cur_boundary, direction[i]);
            LOG(INFO) << "cur boundary:" << cur_boundary;
            if (cur_boundary >= MAX_TOKEN_NUM) {
                break;
            }
        }
        if (cur_boundary >= MAX_TOKEN_NUM || cur_boundary == 0) {
        }
        if (token_in > cur_boundary) {
            token_in = cur_boundary;
        }
        uint256_t token_out = token_in;
        for (uint32_t i = 0; i < path.size(); ++i) {
            token_out = path[i]->compute_output(token_out, direction[i]);
            LOG(INFO) << "token_out:" << token_out;
        }
        LOG(DEBUG) << "token in:" << token_in + accumulate_input << " token_out:" << token_out + accumulate_output;
        uint256_t tmp = token_out + accumulate_output - token_in - accumulate_input;
        if (tmp > max) {
            max = tmp;
            res_in = token_in + accumulate_input;
        }
        // to next round
        accumulate_input += cur_boundary;
        for (uint32_t i = 0; i < path.size(); ++i) {
            cur_boundary = path[i]->process_swap(cur_boundary, direction[i]);
        }
        if (cur_boundary == 0) {
            break;
        }
        accumulate_output += cur_boundary;
        LOG(INFO) << "boundary out: " << accumulate_output;
    }
    //uint32_t token_index = direction[0] ? path[0]->token1 : path[0]->token2;
    //PoolBase* to_eth_pool = 0;
    //uint256_t eth_out = PoolManager::instance()->token_to_eth(token_index, max, &to_eth_pool);
    //SearchResult result {path, res_in, eth_out, to_eth_pool, direction};
    //LOG(INFO) << "compute complete with token_out:" << max << " eth_out:" << eth_out << " token_in:" << res_in;
    return 0;
}