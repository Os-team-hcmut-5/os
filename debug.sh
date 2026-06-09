#!/bin/bash

# Function to display input and run the OS simulation
run_debug_test() {
    local test_name=$1
    echo ""
    echo "==========================================================="
    echo ">>> TARGET: $test_name"
    echo "==========================================================="
    
    echo "[INPUT] Contents of input/$test_name:"
    echo "-----------------------------------------------------------"
    # Print the input file. 
    # Adding a check just in case the file doesn't exist to prevent cat errors.
    if [ -f "input/$test_name" ]; then
        cat "input/$test_name"
    else
        echo "ERROR: input/$test_name not found!"
    fi
    echo ""
    
    echo "-----------------------------------------------------------"
    echo "[OUTPUT] Execution output:"
    echo "-----------------------------------------------------------"
    # Execute the OS and stream directly to the terminal
    ./os "$test_name"
    echo "==========================================================="
}

echo ""
echo "###########################################################"
echo "#         Running MLQ & Paging Tests (DEBUG MODE)         #"
echo "###########################################################"

run_debug_test "os_0_mlq_paging"
run_debug_test "os_1_mlq_paging"
run_debug_test "os_1_mlq_paging_small_1K"
run_debug_test "os_1_mlq_paging_small_4K"
run_debug_test "os_2_mlq_paging"

echo ""
echo "###########################################################"
echo "#          Running Single CPU Tests (DEBUG MODE)          #"
echo "###########################################################"

run_debug_test "os_1_singleCPU_mlq"
run_debug_test "os_1_singleCPU_mlq_paging"
run_debug_test "os_2_singleCPU_mlq_paging"

echo ""
echo "###########################################################"
echo "#               System Call Tests (DEBUG MODE)            #"
echo "###########################################################"

run_debug_test "os_sc"
run_debug_test "os_syscall"
run_debug_test "os_syscall_list"

echo ""
echo "###########################################################"
echo "#               Scheduler Tests (DEBUG MODE)              #"
echo "###########################################################"

run_debug_test "sched"
run_debug_test "sched_0"
run_debug_test "sched_1"

echo ""
echo "DEBUG RUN COMPLETE!"