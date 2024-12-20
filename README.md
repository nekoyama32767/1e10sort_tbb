# Parallel sort for reading int32 from file
## usage

```
./parallel_sort file_name
```
Configure IO thread number as 4
```
./parallel_sort file_name 4
```
!! Don't support CRLF file


## How to compile
### Enviroment
Compiler support C++23 (Recommend gcc-14)

Library: Intel TBB or Boost

AVX2 support (option)

## Compile command
### Use TBB
Linux
```
g++ -O2 parallel_sort.cpp -otbb_sort -std=c++23 -pthread -DPARALLEL -ltbb
```
Windows Msys2 system
```
g++ -O2 parallel_sort.cppp -otbb_sort -std=c++23 -lstdc++exp -pthread -DPARALLEL -ltbb12
```
### Use Boost
```
g++ -O2 parallel_sort.cpp -oboost_sort -std=c++23  -pthread -DBOOST_SORT
```
### Use AVX/SEE To Convert from str to int
Add ` -DAVX_SSE -mavx2` to compile options
```
g++ -O2 -std=c++23 -lpthread parallel_sort.cpp -oboost_avx_sort -lstdc++exp -DBOOST_SORT -DAVX_SSE -mavx2
```
## Use async IO
! Depend boost library
```
g++ parallel_sort_async_io.cpp -std=c++23 -O3 -oboost_sort_async_io -DAVX_SSE -DBOOST_SORT -mavx2 -pthread
```

## Result and example

Linux on Xeon 12core 24Thread
Async IO + Boost + AVX
```
time ./boost_sort_async_io random_10e9.txt 

real    0m2.121s
user    0m21.948s
sys     0m2.572s
```

Windows Msys2 gcc14 on 14900HX(8P16E), 32thread

Tbb
```
time ./sort_tbb random_10e9.txt

real    0m3.993s
user    0m0.000s
sys     0m0.000s
```
AVX/SSE + Boost
```
time ./boost_avx_sort random_10e9.txt 

real    0m3.560s
user    0m0.000s
sys     0m0.015s
```
