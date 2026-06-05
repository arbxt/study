#include <arpa/inet.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct BenchResult {
    std::atomic<int> success{0};
    std::atomic<int> failed{0};
    std::mutex latency_mutex;
    std::vector<long long> latencies_us;
};

bool do_one_request(const std::string& ip, unsigned short port, const std::string& message)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return false;
    }

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        close(fd);
        return false;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        close(fd);
        return false;
    }

    ssize_t sent = send(fd, message.data(), message.size(), 0);
    if (sent != static_cast<ssize_t>(message.size())) {
        close(fd);
        return false;
    }

    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));

    ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
    if (n <= 0) {
        close(fd);
        return false;
    }

    close(fd);

    return std::string(buffer, n) == message;
}

void worker(
    const std::string& ip,
    unsigned short port,
    int requests_per_thread,
    BenchResult& result
)
{
    const std::string message = "hello benchmark";

    for (int i = 0; i < requests_per_thread; ++i) {
        auto begin = std::chrono::steady_clock::now();

        bool ok = do_one_request(ip, port, message);

        auto end = std::chrono::steady_clock::now();

        auto cost_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

        if (ok) {
            ++result.success;

            std::lock_guard<std::mutex> lock(result.latency_mutex);
            result.latencies_us.push_back(cost_us);
        } else {
            ++result.failed;
        }
    }
}

long long percentile(std::vector<long long>& data, double p)
{
    if (data.empty()) {
        return 0;
    }

    std::sort(data.begin(), data.end());

    size_t index = static_cast<size_t>(p * data.size());
    if (index >= data.size()) {
        index = data.size() - 1;
    }

    return data[index];
}

int main(int argc, char* argv[])
{
    if (argc != 5) {
        std::cerr << "用法: "
                  << argv[0]
                  << " <ip> <port> <并发线程数> <每线程请求数>"
                  << std::endl;

        std::cerr << "示例: "
                  << argv[0]
                  << " 127.0.0.1 8080 100 100"
                  << std::endl;
        return 1;
    }

    std::string ip = argv[1];
    unsigned short port = static_cast<unsigned short>(std::stoi(argv[2]));
    int thread_count = std::stoi(argv[3]);
    int requests_per_thread = std::stoi(argv[4]);

    BenchResult result;

    std::vector<std::thread> threads;

    auto total_begin = std::chrono::steady_clock::now();

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(
            worker,
            std::cref(ip),
            port,
            requests_per_thread,
            std::ref(result)
        );
    }

    for (auto& t : threads) {
        t.join();
    }

    auto total_end = std::chrono::steady_clock::now();

    auto total_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            total_end - total_begin
        ).count();

    int total_requests = thread_count * requests_per_thread;
    int success = result.success.load();
    int failed = result.failed.load();

    double seconds = total_ms / 1000.0;
    double qps = seconds > 0 ? success / seconds : 0.0;

    std::vector<long long> latencies;
    {
        std::lock_guard<std::mutex> lock(result.latency_mutex);
        latencies = result.latencies_us;
    }

    long long sum = 0;
    for (long long x : latencies) {
        sum += x;
    }

    long long avg = latencies.empty() ? 0 : sum / latencies.size();
    long long p95 = percentile(latencies, 0.95);
    long long p99 = percentile(latencies, 0.99);

    std::cout << "总请求数: " << total_requests << std::endl;
    std::cout << "成功数: " << success << std::endl;
    std::cout << "失败数: " << failed << std::endl;
    std::cout << "总耗时(ms): " << total_ms << std::endl;
    std::cout << "QPS: " << qps << std::endl;
    std::cout << "平均延迟(us): " << avg << std::endl;
    std::cout << "P95延迟(us): " << p95 << std::endl;
    std::cout << "P99延迟(us): " << p99 << std::endl;

    return 0;
}