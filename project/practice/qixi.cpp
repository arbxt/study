// File: qixi_gacha.cpp
// Build: C++17 or later
// Usage examples:
//   ./qixi_gacha                      # 交互抽一次，可继续
//   ./qixi_gacha --multi 100000       # 模拟 10 万次并统计
//   ./qixi_gacha --until-ssr          # 抽到“好呀宝宝”为止
//   ./qixi_gacha --seed 42 --multi 1e6
//
// Notes (why):
// - 使用 discrete_distribution 严格按权重（概率）抽样。
// - Deadline: 超过 2025-08-29（含）后拒绝抽奖，符合活动期。
// - UTF-8 输出；如 Windows 控制台乱码，建议使用支持 UTF-8 的终端。

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <optional>

struct Prize {
    std::string name;
    double weight; // probability weight (sum = 100)
};

struct Config {
    bool interactive = true;
    bool until_ssr = false;
    uint64_t multi = 0; // 0 表示不批量
    std::optional<uint64_t> seed;
};

static const std::vector<Prize> kPrizes = {
    {"滚你妈的", 15.0},
    {"去你妈的", 10.0},
    {"你也配？", 10.0},
    {"傻逼二次元", 10.0},
    {"癔症又犯了？", 10.0},
    {"现在是幻想时刻", 5.0},
    {"柚子厨滚出去", 5.0},
    {"这是最后通牒", 5.0},
    {"以后不要再和我扯上关系", 5.0},
    {"被拉黑+屏蔽", 5.0},
    {"小男娘", 5.0},
    {"随机的网图", 2.5},
    {"随机的贴吧圣经", 2.5},
    {"下次再说吧", 2.5},
    {"你是个好人", 2.5},
    {"我一直把你当好朋友", 2.5},
    {"滚出去", 2.49},
    {"好呀宝宝", 0.01}
};

class Gacha {
  public:
    explicit Gacha(std::mt19937_64 rng) : rng_(std::move(rng)) {
        std::vector<double> w; w.reserve(kPrizes.size());
        for (const auto &p : kPrizes) w.push_back(p.weight);
        dist_ = std::discrete_distribution<int>(w.begin(), w.end());
    }

    // 返回抽到的奖项索引
    int draw_one() { return dist_(rng_); }

  private:
    std::mt19937_64 rng_;
    std::discrete_distribution<int> dist_;
};

static inline bool check_deadline() {
    // 活动截至日期：2025-08-29 23:59:59 本地时间（含当日）
    std::tm deadline_tm{};
    deadline_tm.tm_year = 2025 - 1900;
    deadline_tm.tm_mon  = 7;  // 0-based: 7 = August
    deadline_tm.tm_mday = 29;
    deadline_tm.tm_hour = 23;
    deadline_tm.tm_min  = 59;
    deadline_tm.tm_sec  = 59;

    std::time_t deadline = std::mktime(&deadline_tm);
    std::time_t now = std::time(nullptr);
    return now <= deadline;
}

static inline void print_prize_table() {
    std::cout << "\n—— 抽奖概率（综合概率）——\n";
    std::cout << std::left << std::setw(24) << "奖项" << std::right << std::setw(10) << "概率(%)" << "\n";
    std::cout << std::string(34, '-') << "\n";
    double total = 0.0;
    for (const auto &p : kPrizes) {
        std::cout << std::left << std::setw(24) << p.name
                  << std::right << std::setw(10) << std::fixed << std::setprecision(2) << p.weight << "\n";
        total += p.weight;
    }
    std::cout << std::string(34, '-') << "\n";
    std::cout << std::left << std::setw(24) << "合计" << std::right << std::setw(10) << std::fixed << std::setprecision(2) << total << "\n\n";
}

static Config parse_args(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--multi" && i + 1 < argc) {
            cfg.interactive = false;
            cfg.multi = static_cast<uint64_t>(std::stoull(argv[++i]));
        } else if (arg == "--until-ssr") {
            cfg.interactive = false;
            cfg.until_ssr = true;
        } else if (arg == "--seed" && i + 1 < argc) {
            cfg.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "用法: ./qixi_gacha [--multi N] [--until-ssr] [--seed S]\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("未知参数: " + arg);
        }
    }
    return cfg;
}

static std::mt19937_64 make_rng(const std::optional<uint64_t>& seed) {
    if (seed) return std::mt19937_64(*seed);
    std::random_device rd;
    std::seed_seq seq{
        rd(), rd(), static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count() & 0xffffffffu)
    };
    return std::mt19937_64(seq);
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

#if defined(_WIN32)
    // Windows 控制台可能默认非 UTF-8，建议在外部设置 chcp 65001。
#endif

    try {
        Config cfg = parse_args(argc, argv);

        if (!check_deadline()) {
            std::cerr << "活动已结束（截至 2025-08-29）。" << std::endl;
            return 1;
        }

        auto rng = make_rng(cfg.seed);
        Gacha g(std::move(rng));

        if (cfg.interactive) {
            print_prize_table();
            std::cout << "发送口令以抽取：\"咱俩试试？\" (输入 q 退出)\n"<<std::flush;
            std::string line;
            while (std::getline(std::cin, line)) {
                if (line == "q" || line == "quit" || line == "exit") break;
                if(line == "咱俩试试？") {
                    std::cout << "口令正确，开始抽奖...\n" << std::flush;
                    int idx = g.draw_one();
                    std::cout << "🎉 结果：" << kPrizes[idx].name << "\n" << std::flush;
                    return 0;
                } else {
                    std::cout << "口令错误，请输入：咱俩试试？\n> ";
                    continue;
                }
            }
            return 0;
        }

        if (cfg.until_ssr) {
            const int SSR = static_cast<int>(kPrizes.size() - 1); // “好呀宝宝”
            uint64_t cnt = 0;
            auto t0 = std::chrono::high_resolution_clock::now();
            int idx = -1;
            do {
                idx = g.draw_one();
                ++cnt;
            } while (idx != SSR);
            auto t1 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> dt = t1 - t0;
            std::cout << "抽到 \"" << kPrizes[SSR].name << "\" 共用次数：" << cnt << ", 耗时 " << std::fixed << std::setprecision(3) << dt.count() << "s\n";
            return 0;
        }

        if (cfg.multi > 0) {
            std::vector<uint64_t> freq(kPrizes.size());
            auto t0 = std::chrono::high_resolution_clock::now();
            for (uint64_t i = 0; i < cfg.multi; ++i) {
                ++freq[g.draw_one()];
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> dt = t1 - t0;

            std::cout << "模拟次数：" << cfg.multi << ", 耗时 " << std::fixed << std::setprecision(3) << dt.count() << "s\n";
            std::cout << std::left << std::setw(24) << "奖项" << std::right
                      << std::setw(14) << "期望(%)" << std::setw(14) << "实测(%)" << std::setw(14) << "次数" << "\n";
            std::cout << std::string(66, '-') << "\n";
            for (size_t i = 0; i < kPrizes.size(); ++i) {
                double expected = kPrizes[i].weight;
                double observed = 100.0 * static_cast<long double>(freq[i]) / static_cast<long double>(cfg.multi);
                std::cout << std::left << std::setw(24) << kPrizes[i].name
                          << std::right << std::setw(14) << std::fixed << std::setprecision(2) << expected
                          << std::setw(14) << std::fixed << std::setprecision(4) << observed
                          << std::setw(14) << freq[i]
                          << "\n";
            }
            return 0;
        }
    } catch (const std::exception& ex) {
        std::cerr << "错误: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
