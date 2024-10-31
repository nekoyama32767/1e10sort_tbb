# Parallel sort for reading int32 from file
## usage
```
./tbb_sort file_name
```
## How to compile
### Enviroment
Compiler support C++23 (Recommend gcc-14)

Library: Intel TBB
## Compile command
Linux
```
g++ -O2 sort_tbb.cpp -otbb_sort -std=c++23 -lstdc++exp -pthread -DPARALLEL -ltbb
```
Windows Msys2 system
```
g++ -O2 sort_tbb.cpp -otbb_sort -std=c++23 -lstdc++exp -pthread -DPARALLEL -ltbb12
```

## Result and example
Windows Msys2 gcc14 on 14900HX(8P16E), 32thread
```
time ./sort_tbb random_10e9.txt

real    0m3.993s
user    0m0.000s
sys     0m0.000s
```