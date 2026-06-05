#!/bin/bash

echo "====================================="
echo " Compiling the Operating System...   "
echo "====================================="
make clean
make all

echo ""
echo "====================================="
echo " Creating output directory...        "
echo "====================================="
mkdir -p output

echo ""
echo "====================================="
echo " Running Scheduling Tests...         "
echo "====================================="

# echo "Running sched..."
# ./os sched > output/sched.output

echo "Running sched_0..."
./os sched_0 > output/sched_0.output

echo "Running sched_1..."
./os sched_1 > output/sched_1.output


echo ""
echo "====================================="
echo " Running MLQ & Paging Tests...       "
echo "====================================="
echo "Running os_0_mlq_paging..."
./os os_0_mlq_paging > output/os_0_mlq_paging.output

echo "Running os_1_mlq_paging..."
./os os_1_mlq_paging > output/os_1_mlq_paging.output

echo "Running os_1_mlq_paging_small_1K..."
./os os_1_mlq_paging_small_1K > output/os_1_mlq_paging_small_1K.output

echo "Running os_1_mlq_paging_small_4K..."
./os os_1_mlq_paging_small_4K > output/os_1_mlq_paging_small_4K.output

echo "Running os_2_mlq_paging..."
./os os_2_mlq_paging > output/os_2_mlq_paging.output


echo ""
echo "====================================="
echo " Running Single CPU Tests...         "
echo "====================================="
echo "Running os_1_singleCPU_mlq..."
./os os_1_singleCPU_mlq > output/os_1_singleCPU_mlq.output

echo "Running os_1_singleCPU_mlq_paging..."
./os os_1_singleCPU_mlq_paging > output/os_1_singleCPU_mlq_paging.output

echo "Running os_2_singleCPU_mlq_paging..."
./os os_2_singleCPU_mlq_paging > output/os_2_singleCPU_mlq_paging.output


echo ""
echo "====================================="
echo " Running Custom & Syscall Tests...   "
echo "====================================="
echo "Running os_sc..."
./os os_sc > output/os_sc.output

echo "Running os_syscall..."
./os os_syscall > output/os_syscall.output

echo "Running os_syscall_list..."
./os os_syscall_list > output/os_syscall_list.output

echo ""
echo "====================================="
echo " SUCCESS! All tests have finished!   "
echo "====================================="