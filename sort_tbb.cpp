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
#include <execution>
#endif


constexpr auto TIME = 100000000;
int32_t thread_num = std::thread::hardware_concurrency();

auto file_separator_pos(std::string_view file_sv) -> std::vector<size_t> 
{
    std::vector<size_t> ret;
    ret.push_back(0);
    std::ifstream fin(file_sv.data(), std::ifstream::ate);
    size_t end_pos = static_cast<size_t>(fin.tellg());

    size_t offset = 0;
    size_t split_size = end_pos / thread_num;
    for(auto i:std::views::iota(0, thread_num))
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

auto range_conv(std::string_view file_sv, size_t begin, size_t end, std::vector<int32_t> &dest)
{
    dest.reserve((TIME/ thread_num) * 3 / 2);
    std::ifstream fin(file_sv.data());
    fin.seekg(begin);
    while (fin.tellg() < end)
    {
        std::array<char, 16> buf;
        fin.getline(&buf[0], 16);
        int32_t val;
        if (auto [ptr, ec] = std::from_chars(buf.begin(), buf.end(), val); ec == std::errc{}) 
        {
            dest.push_back(val);
        }
    }
}

auto workers_conv_file2vecs(std::string_view file_sv) -> std::vector<std::vector<int32_t>>
{
    std::vector<std::thread> threads;
    std::vector<std::vector<int32_t>> workers_conv_vecs(thread_num);
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
    auto conv_vecs = workers_conv_file2vecs(file_sv);
    dest.reserve(TIME);
    int i = 0;
    int total = 0;
    for (auto &vec:conv_vecs)
    {
        total += vec.size();
        for (auto v:vec)
        {
            dest.push_back(v);
        }
    }
#ifdef PARALLEL
    std::sort(std::execution::par, dest.begin(), dest.end());
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
    for (auto &t:threads)
    {
        t.join();
    }
    std::ofstream fout("sort.txt", std::ios::binary);
    for (auto &buf:workers_out_put_buffers)
    {
        fout << buf.data();
    }
}

std::vector<int32_t> source;
int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::println("Wrong argument, only accept one file path");
        return 0;
    }
    std::string file_name;
    file2vec_sort_multi(argv[1], source);
    vec2file_multi(source, "sort.txt");
    return 0;
}