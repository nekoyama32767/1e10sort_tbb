#include <random>
#include <format>
#include <fstream>
#include <print>
#include <ranges>
#include <string>
#include <thread>
#include <charconv>
#include <array>
#include <iostream>
#include <algorithm>
#include <span>
#include <spanstream>
#ifdef PARALLEL
#define _PSTL_PAR_BACKEND_TBB
#include <execution>
//#include <tbb/parallel_sort.h>
#endif
#ifdef BOOST_SORT
#include <boost/sort/block_indirect_sort/block_indirect_sort.hpp>
#endif
#ifdef AVX_SSE
#include <immintrin.h>
#endif
#ifdef PARALLEL_RADIX
#include <parallel_radix_sort.h>
#endif
#include <boost/lockfree/queue.hpp>

constexpr auto TIME = 100000000;
int32_t thread_num = std::thread::hardware_concurrency();

auto file_separator_pos(std::string_view file_sv, int32_t default_split_num = thread_num) -> std::vector<size_t> 
{
    std::vector<size_t> ret;
    ret.push_back(0);
    std::ifstream fin(file_sv.data(), std::ifstream::ate);
    size_t end_pos = static_cast<size_t>(fin.tellg());

    size_t offset = 0;
    size_t split_size = end_pos / default_split_num - 1;
    for(auto i:std::views::iota(0, default_split_num))
    {
        if (offset + split_size >= end_pos)
        {
            ret.push_back(end_pos);
        }
        else
        {
            fin.seekg(offset + split_size);
            while (fin.get() != '\n');
            offset = fin.tellg();
            ret.push_back(offset);
        }
    }
    return ret;
}

auto check_separator_print(std::string_view file_sv, size_t begin, size_t end)
{
    std::ifstream fin(file_sv.data());
    fin.seekg(begin);
    while (fin.tellg() < end)
    {
        std::string st;
        std::getline(fin, st);
        std::println("{}", st);
    }
}
#ifdef AVX_SSE
inline uint32_t strToUintSSE(char* sta) {
    //Set up constants
    __m128i zero        = _mm_setzero_si128();
    __m128i multiplier1 = _mm_set_epi16(1000,100,10,1,1000,100,10,1);
    __m128i multiplier2 = _mm_set_epi32(0, 100000000, 10000, 1);

    //Compute length of string
    __m128i string      = _mm_lddqu_si128((__m128i*)sta);

    __m128i digitRange  = _mm_setr_epi8('0','9',0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    int len = _mm_cmpistri(digitRange, string, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);


    sta += len + 1;

    //Reverse order of number
    __m128i permutationMask = _mm_set1_epi8(len);
    permutationMask = _mm_add_epi8(permutationMask, _mm_set_epi8(-16,-15,-14,-13,-12,-11,-10,-9,-8,-7,-6,-5,-4,-3,-2,-1));
    string = _mm_shuffle_epi8(string, permutationMask);

    //Shift string down
    __m128i zeroChar = _mm_set1_epi8('0');
    string = _mm_subs_epu8(string, zeroChar);

    //Multiply with right power of 10 and add up
    __m128i stringLo = _mm_unpacklo_epi8(string, zero);
    __m128i stringHi = _mm_unpackhi_epi8(string, zero);
    stringLo = _mm_madd_epi16(stringLo, multiplier1);
    stringHi = _mm_madd_epi16(stringHi, multiplier1);
    __m128i intermediate = _mm_hadd_epi32(stringLo, stringHi);
    intermediate = _mm_mullo_epi32(intermediate, multiplier2);

    //Hadd the rest up
    intermediate = _mm_add_epi32(intermediate, _mm_shuffle_epi32(intermediate, 0b11101110));
    intermediate = _mm_add_epi32(intermediate, _mm_shuffle_epi32(intermediate, 0b01010101));

    return _mm_cvtsi128_si32(intermediate);
}
#endif

#ifdef USE_RADIX_PRE

#ifndef radix_sort_cpp
#define radix_sort_cpp

#include <iterator>
#include <vector>

template <int UNIT, int BITS, class RAI>
void radix_sort(RAI a0, RAI aN)
{
        typedef typename std::iterator_traits<RAI>::value_type val_t;
        typedef typename std::iterator_traits<RAI>::difference_type dif_t;
        static const int KEYS = 1 << UNIT;
        static const int MASK = KEYS - 1;
        const dif_t N = aN - a0;
        const RAI& a = a0;

        if (N < 2) return;
        std::vector<dif_t> h(KEYS);
        std::vector<val_t> b(N);
        const typename std::vector<val_t>::iterator b0 = b.begin();
        const typename std::vector<val_t>::iterator bN = b.end();
        for (int shift = 0; shift < BITS; shift += UNIT) {
                for (int k = 0; k < KEYS; k++) h[k] = 0;
                typename std::vector<val_t>::iterator bi = b0;
                bool done = true;
                for (RAI ai = a0; ai < aN; ++ai, ++bi) {
                        const val_t& x = *ai;
                        const int32_t y = (x) >> shift;
                        if (y > 0) done = false;
                        ++h[int(y & MASK)];
                        *bi = x;
                }
                if (done) return; 
                for (int32_t k = 1; k < KEYS; k++) h[k] += h[k - 1];
                for (bi = bN; bi > b0;) { 
                        const val_t& x = *(--bi);
                        const int32_t y = int32_t(((x) >> shift) & MASK);
                        const dif_t j = --h[y];
                        a[j] = x;
                }
        }
}

#endif // radix_sort_cpp
#endif //USE_RADIX_PRE

struct buffer_block{
    std::vector<char> data_vec;
    size_t size;
};

int32_t io_threads{8};
int32_t BUFFER_BLOCK_NUM = 256;
int32_t BUFFER_BLOCK_SIZE = static_cast<size_t>(TIME / 24) / 32ull * 5ull * 11ull;
std::vector<buffer_block> buffer_blocks(BUFFER_BLOCK_NUM);
boost::lockfree::queue<int32_t> conv_task_que(BUFFER_BLOCK_NUM);
boost::lockfree::queue<int32_t> conv_task_reuse_que(BUFFER_BLOCK_NUM);
//constexpr auto 

auto file2vec_block_init()
{
    for (auto i:std::views::iota(0, BUFFER_BLOCK_NUM))
    {
        buffer_blocks[i].data_vec.resize(BUFFER_BLOCK_SIZE + 10, 0);
        while(!conv_task_reuse_que.push(i));
    }
}

auto file2vec_block_read_worker(std::string_view file_sv, size_t begin, size_t end)
{
    std::ifstream fin(file_sv.data());
    fin.seekg(begin);
    size_t offset = BUFFER_BLOCK_SIZE;
    size_t now = fin.tellg();
    while(fin.tellg() < end)
    {
        offset = BUFFER_BLOCK_SIZE;
        if (now + offset >= end)
        {
            offset = end - now;
        }
        else
        {
            fin.seekg(now + offset);
            while (fin.get() != '\n'){};
            offset = static_cast<size_t>(fin.tellg()) - now;
            fin.seekg(now);
        }
        for (;;)
        {
             int32_t buf_id = 1;
            if (conv_task_reuse_que.pop(buf_id))
            {
                auto &block = buffer_blocks[buf_id];
                fin.read(block.data_vec.data(), offset);
                block.size = offset;
                now += offset;
                while(!conv_task_que.push(buf_id));
                break;
            }
        }
    }
}
std::atomic_bool consumer_stop_flag{false};
auto conv_consumer() -> std::vector<int32_t>
{
    std::vector<int32_t> ret;
    ret.reserve((TIME/ thread_num) / 4 * 5);
    for (;;)
    {
        if (consumer_stop_flag.load())
        {
            break;
        }
        int32_t buf_id = 1;
        if (conv_task_que.pop(buf_id))
        {
            std::vector<char> buf_con;
            buf_con.reserve(16);
            auto &block = buffer_blocks[buf_id];
            for (auto word:std::views::split(std::string_view{block.data_vec.data()}, '\n'))
            {
                auto sv = std::string_view(word);
                if (sv.size() == 0)
                {
                    break;
                }
                std::copy(sv.begin(), sv.end(), std::back_inserter(buf_con));
                buf_con.push_back(0);
                #ifdef AVX_SSE
                ret.push_back(static_cast<int32_t>(strToUintSSE(buf_con.data())));
                #else
                int32_t val = 0;
                if (auto [ptr, ec] = std::from_chars(buf_con.data(), buf_con.data() + sv.size(), val); ec == std::errc{}) 
                {
                    ret.push_back(val);
                }
                #endif
                //std::println("{} pair\n", buf_con.data());
                buf_con.clear();
            }
            std::ranges::fill(block.data_vec, 0);
            block.size = 0;
            while(!conv_task_reuse_que.push(buf_id));
        }
    }
    return ret;
}

auto file2vec_block(std::string_view file_sv) -> std::vector<std::future<std::vector<int32_t>>>
{
    std::vector<std::future<std::vector<int32_t>>> workers_conv_vecs;
    std::vector<std::thread> threads;
    //std::vector<std::thread> threads_dummy;
    file2vec_block_init();
    auto separator_pos = file_separator_pos(file_sv, io_threads);
   for (auto [idx, sp]: separator_pos | std::views::slide(2) | std::views::enumerate)
    {
        threads.emplace_back(std::thread{file2vec_block_read_worker, file_sv, sp[0], sp[1]});
    }

    for (auto i:std::views::iota(0, thread_num))
    {
        //threads_dummy.emplace_back(std::thread{conv_consumer, std::ref(workers_conv_vecs[i])});
        workers_conv_vecs.emplace_back(std::async(std::launch::async, (conv_consumer)));
    }

    for(auto &t:threads)
    {
        t.join();
    }
    while (!conv_task_que.empty()) {std::this_thread::yield();};
    consumer_stop_flag = true;
    // for(auto &t:threads_dummy)
    // {
    //     t.join();
    // }
    //file2vec_block_read_worker(file_sv, 0, separator_pos.back());
    return workers_conv_vecs;
}

auto range_conv(std::string_view file_sv, size_t begin, size_t end, std::vector<int32_t> &dest)
{
    dest.reserve((TIME/ thread_num) / 4 * 5);
    std::ifstream fin(file_sv.data());
    fin.seekg(begin);
    while (fin.tellg() < end)
    {
        std::array<char, 16> buf;
        fin.getline(&buf[0], 16);
        //auto val = 0;
        //std::println("{} {}", val, buf.data());
        #ifdef AVX_SSE
        dest.push_back(static_cast<int32_t>(strToUintSSE(buf.data())));
        #else
        int32_t val = 0;
        if (auto [ptr, ec] = std::from_chars(buf.begin(), buf.end(), val); ec == std::errc{}) 
        {
            dest.push_back(val);
        }
        #endif
    }
#ifdef USE_RADIX_PRE
    radix_sort<8, 32>(dest.begin(), dest.end());
#endif
}

auto workers_conv_file2vecs(std::string_view file_sv) -> std::vector<std::vector<int32_t>>
{
    std::vector<std::thread> threads;
    std::vector<std::vector<int32_t>> workers_conv_vecs(thread_num);
    std::println("{}", 111);
    auto separator_pos = file_separator_pos(file_sv);
    for (auto [idx, sp]: separator_pos | std::views::slide(2) | std::views::enumerate)
    {
        threads.emplace_back(std::thread{range_conv, file_sv, sp[0], sp[1], std::ref(workers_conv_vecs[idx])});
    }
    for (auto &t:threads)
    {
        t.join();
    }
    return workers_conv_vecs;
}

auto file2vec_sort_multi(std::string_view file_sv, std::vector<int32_t> &dest)
{
    std::vector<std::thread> threads;
    //auto conv_vecs = workers_conv_file2vecs(file_sv);
    auto conv_vecs = file2vec_block(file_sv);
    dest.clear();
    dest.resize(TIME);
    size_t len = 0;
    int i = 0;
    int total = 0;
    std::vector<bool> con_checked(thread_num);
    std::vector<std::vector<int32_t>> workers_conv_vecs_recv(thread_num);
    for (int i{}; i < thread_num;)
    {
        for (auto tid:std::views::iota(0, thread_num))
        {
            if (con_checked[tid]) continue;
            std::future_status status;
            status = conv_vecs[tid].wait_for(std::chrono::milliseconds(10));
            if (status == std::future_status::ready)
            {
                con_checked[tid] = true;
                ++i;
                workers_conv_vecs_recv[tid] = conv_vecs[tid].get();
                auto& vec = workers_conv_vecs_recv[tid];
                if (vec.size() > 0)
                {
                    auto beg = len;
                    len += vec.size();
                    threads.emplace_back(std::thread{[](auto tid, auto beg, auto end, auto dest){
                        std::copy(beg, end, dest);
                    }, tid, vec.begin(), vec.end() , dest.begin() + beg});
                }
            }
        }
    }
    std::ranges::for_each(threads, [](auto &t){t.join();});
    dest.resize(len);
#ifdef PARALLEL
    std::sort(std::execution::par_unseq, dest.begin(), dest.end());
    //tbb::parallel_sort(dest.begin(), dest.end());
#elif defined(BOOST_SORT)
    boost::sort::block_indirect_sort(dest.begin(), dest.end());
#elif defined(PARALLEL_RADIX)
    parallel_radix_sort::SortKeys(dest.data(), dest.size());
#else
    std::ranges::sort(dest);
#endif
}

auto workers_make_outputs(std::vector<int32_t> &source, size_t beg, size_t end, std::vector<char>& buf)
{
    buf.resize(static_cast<size_t>(TIME / thread_num) / 4ull * 5ull * 11ull);
    std::ospanstream ost{std::span{buf}};
    for (auto d:source | std::views::drop(beg) | std::views::take(end - beg))
    {
        ost << d << "\n";
    }
}
auto vec2file_multi(std::vector<int32_t> &source, std::string_view file_sv)
{
    std::vector<std::thread> threads;
    std::vector<std::vector<char>> workers_out_put_buffers(thread_num);
    size_t offset = 0;
    size_t tail = source.size() % thread_num;
    size_t split= source.size() / thread_num;
    for (auto i:std::views::iota(0,thread_num))
    {
        size_t k = 0;
        if (i < tail)
        {
            k = 1;
        }
        threads.emplace_back(std::thread{workers_make_outputs, std::ref(source), offset, offset + split + k, std::ref(workers_out_put_buffers[i])});
        offset += split + k;
    }
    std::ofstream fout("sort.txt", std::ios::binary);
    for (auto i:std::views::iota(0,thread_num))
    {
        threads[i].join();
        fout << workers_out_put_buffers[i].data();
    }
    // for (auto &buf:workers_out_put_buffers)
    // {
    //     fout << buf.data();
    // }
}

std::vector<int32_t> source;
int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::println("Wrong argument useage  filename  io_thread_num(optional) ");
        return 0;
    }
    else
    {
        if (argc >= 3)
        {
            io_threads = std::stoi(std::string(argv[2]));
        }
    }

    //file2vec_block(argv[1]);
    file2vec_sort_multi(argv[1], source);
    vec2file_multi(source, "sort.txt");
    return 0;
}