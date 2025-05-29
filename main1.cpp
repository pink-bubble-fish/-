#include "PCFG.h"
#include <chrono>
#include <fstream>
#include "md5.h"
#include <iomanip>
#include <pthread.h>
#include <mutex>
using namespace std;
using namespace chrono;

// 编译指令如下
// g++ main.cpp train.cpp guessing.cpp md5.cpp -o main
// g++ main.cpp train.cpp guessing.cpp md5.cpp -o main -O1
// g++ main.cpp train.cpp guessing.cpp md5.cpp -o main -O2
std::mutex mtx;
struct ThreadData {
    size_t start_index;
    size_t end_index;
    vector<string> *guesses;
};

void* hash_guesses(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    bit32 state[4];
    for (int i = data->start_index; i < data->end_index; ++i) {
        string pw = (*data->guesses)[i];  // 
        mtx.lock();
        MD5Hash(pw, state); // 调用 MD5 哈希函数
        mtx.unlock(); 
    }
    return nullptr;
}

int main()
{
    double time_hash = 0;  // 用于MD5哈希的时间
    double time_guess = 0; // 哈希和猜测的总时长
    double time_train = 0; // 模型训练的总时长
    PriorityQueue q;
    auto start_train = system_clock::now();
    q.m.train("/guessdata/Rockyou-singleLined-full.txt");
    q.m.order();
    auto end_train = system_clock::now();
    auto duration_train = duration_cast<microseconds>(end_train - start_train);
    time_train = double(duration_train.count()) * microseconds::period::num / microseconds::period::den;

    q.init();
    cout << "here" << endl;
    int curr_num = 0;
    auto start = system_clock::now();
    // 由于需要定期清空内存，我们在这里记录已生成的猜测总数
    int history = 0;
    // std::ofstream a("./files/results.txt");
    while (!q.priority.empty())
    {
        q.PopNext();
        q.total_guesses = q.guesses.size();
        if (q.total_guesses - curr_num >= 100000)
        {
            cout << "Guesses generated: " <<history + q.total_guesses << endl;
            curr_num = q.total_guesses;

            // 在此处更改实验生成的猜测上限
            int generate_n=10000000;
            if (history + q.total_guesses > 10000000)
            {
                auto end = system_clock::now();
                auto duration = duration_cast<microseconds>(end - start);
                time_guess = double(duration.count()) * microseconds::period::num / microseconds::period::den;
                cout << "Guess time:" << time_guess - time_hash << "seconds"<< endl;
                cout << "Hash time:" << time_hash << "seconds"<<endl;
                cout << "Train time:" << time_train <<"seconds"<<endl;
                break;
            }
        }
        // 为了避免内存超限，我们在q.guesses中口令达到一定数目时，将其中的所有口令取出并且进行哈希
        // 然后，q.guesses将会被清空。为了有效记录已经生成的口令总数，维护一个history变量来进行记录
        if (curr_num > 1000000)
        {
            auto start_hash = system_clock::now();
            bit32 state[4];
            size_t num_threads = 4;  // 使用4个线程
            pthread_t threads[num_threads];
            vector<ThreadData> thread_data;
            size_t chunk_size = q.guesses.size() / num_threads;

            // 创建线程，每个线程处理一个口令子集
            for (size_t i = 0; i < num_threads; ++i) {
                thread_data.push_back(ThreadData{
                    i * chunk_size,
                    (i == num_threads - 1) ? q.guesses.size() : (i + 1) * chunk_size,
                    &q.guesses // 将 q.guesses 的地址传递给线程
                });
                pthread_create(&threads[i], nullptr, hash_guesses, (void*)&thread_data[i]);
            }


            // 等待所有线程完成
            for (size_t i = 0; i < num_threads; ++i) {
                pthread_join(threads[i], nullptr);
            }

            // 在这里对哈希所需的总时长进行计算
            auto end_hash = system_clock::now();
            auto duration = duration_cast<microseconds>(end_hash - start_hash);
            time_hash += double(duration.count()) * microseconds::period::num / microseconds::period::den;

            // 记录已经生成的口令总数
            history += curr_num;
            curr_num = 0;
            q.guesses.clear();
        }
    }
}
