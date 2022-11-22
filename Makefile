
CC=gcc

all:	test_memory_subsystem test_l1 test_l2 test_main_memory

test_memory_subsystem:	test_memory_subsystem.o memory_subsystem.o l1_cache.o l2_cache.o main_memory.o
		gcc -o test_memory_subsystem test_memory_subsystem.o memory_subsystem.o l1_cache.o l2_cache.o main_memory.o

test_l1:	test_l1.o l1_cache.o
	gcc  -o test_l1 test_l1.o l1_cache.o

test_l2:	test_l2.o l2_cache.o
	gcc  -o test_l2 test_l2.o l2_cache.o

test_main_memory:	test_main_memory.o main_memory.o
	gcc  -o test_main_memory test_main_memory.o main_memory.o


ben:	ben_test_memory_subsystem ben_test_l1 ben_test_l2 ben_test_main_memory

ben_test_memory_subsystem:	test_memory_subsystem.o ben_memory_subsystem.o ben_l1_cache.o ben_l2_cache.o ben_main_memory.o
		gcc -o ben_test_memory_subsystem test_memory_subsystem.o ben_memory_subsystem.o ben_l1_cache.o ben_l2_cache.o ben_main_memory.o

ben_test_l1:	test_l1.o ben_l1_cache.o
	gcc  -o ben_test_l1 test_l1.o ben_l1_cache.o

ben_test_l2:	test_l2.o ben_l2_cache.o
	gcc  -o ben_test_l2 test_l2.o ben_l2_cache.o

ben_test_main_memory:	test_main_memory.o ben_main_memory.o
	gcc  -o ben_test_main_memory test_main_memory.o ben_main_memory.o


