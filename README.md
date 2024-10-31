# Parallel sort for reading int32 from file
## usage

```
./parallel_sort file_name
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

### Use naive radix sort on per reading woker and naive merge

```
g++ parallel_sort_radix.cpp -oparallel_radix -O2 -std=c++23 -DAVX_SEE
```

## Result and example
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
Naive Radix per-thread and merge

```
time ./parallel_radix random_1e9.txt

real    0m4.069s
user    0m0.000s
sys     0m0.000s
```