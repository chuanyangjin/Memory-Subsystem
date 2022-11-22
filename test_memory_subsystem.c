

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_subsystem_constants.h"
#include "memory_subsystem.h"

// We'll test with a 32MB (2^25) memory
#define MAIN_MEMORY_SIZE_IN_BYTES (1 << 25)

extern uint32_t num_l1_misses;
extern uint32_t num_l2_misses;


int main()
{

  printf("Initializing memory subsystem\n");
  memory_subsystem_initialize(MAIN_MEMORY_SIZE_IN_BYTES);
  
  //Pass 1: Writing a value to every word

  printf("Pass 1: Writing a value to every word in memory\n");

  uint32_t address;

  uint32_t num_memory_accesses = 0;

  for(address = 0; address < MAIN_MEMORY_SIZE_IN_BYTES; address+=4) {
    //    printf("Address = %u\n", address);
    memory_access(address, address >> 2, WRITE_ENABLE_MASK, NULL);
    num_memory_accesses++;
  }

  printf("In Pass 1, number of memory accesses = %d\n", num_memory_accesses);
  printf("In Pass 1, number of L1 misses = %d\n", num_l1_misses);
  printf("In Pass 1, number of L2 misses = %d\n", num_l2_misses);


  num_l1_misses = 0;
  num_l2_misses = 0;

  num_memory_accesses = 0;

  printf("Pass 2: Reading every word in memory and checking the value\n");

  uint32_t read_data;

  for(address = 0; address < MAIN_MEMORY_SIZE_IN_BYTES; address+=4) {
    memory_access(address, 0, READ_ENABLE_MASK, &read_data);
    num_memory_accesses++;

    if (read_data != (address >> 2)) {
      printf("Error: Value read at address %u is %u, should be %u\n", 
	     address, read_data,address>>2);
      exit(1);
    }
  }

  printf("In Pass 2, number of memory accesses = %d\n", num_memory_accesses);
  printf("In Pass 2, number of L1 misses = %d\n", num_l1_misses);
  printf("In Pass 2, number of L2 misses = %d\n", num_l2_misses);

  printf("Pass 3: Randomly reading and writing words in memory (poor cache performance)\n");

  srand(12345);  //not a random seed, since we want reproducible results.

  num_l1_misses = 0;
  num_l2_misses = 0;

  num_memory_accesses = 0;

#define NUM_TEST_ACCESSES (1<<22)

  uint32_t i = 0;
  while(i<NUM_TEST_ACCESSES) {

    num_memory_accesses++;
    address = (rand()%MAIN_MEMORY_SIZE_IN_BYTES) & ~0x3; //address within memory, on a word boundary.

    if (rand()%2) { //randomly choose to read or write
      //reading
      memory_access(address, 0, READ_ENABLE_MASK, &read_data);
    }
    else {
      //writing
      memory_access(address, (1<<20) - address, WRITE_ENABLE_MASK, NULL);      
    }
    
    i++;
    //Generate a clock interrupt (to clear the r bits in L2) every 8K (= 2^13) memory accesses.
    //This will happen when the lowest 13 bits of i are 0.
    //To check, use the binary mask containing 13 ones:  1 1111 1111 1111 = 1FFF hex
    if (!(i&0x1fff)) { //
      memory_handle_clock_interrupt();
    }
  }
  
  printf("In Pass 3, number of memory accesses = %d\n", num_memory_accesses);
  printf("In Pass 3, number of L1 misses = %d\n", num_l1_misses);
  printf("In Pass 3, number of L2 misses = %d\n", num_l2_misses);

  printf("Passed\n");


  printf("Pass 4: Reading and writing random-length sequences of addresses (better cache performance)\n");

  srand(54321);  //not a random seed, since we want reproducible results.

  num_l1_misses = 0;
  num_l2_misses = 0;

  num_memory_accesses = 0;

  i = 0;
  uint32_t j;

#define LONGEST_SEQUENCE 1000

  uint32_t sequence_length;

  while(i<NUM_TEST_ACCESSES) {

    //choose a sequence of consecutive words to read and write

    sequence_length = rand() % LONGEST_SEQUENCE; 
    
    address = (rand()%MAIN_MEMORY_SIZE_IN_BYTES) & ~0x3; //address within memory, on a word boundary.

    //now perform the sequence of reads or writes, but don't let the addresses go past the memory size
    for(j=0;(j<sequence_length) && ((address+(j<<2)) < MAIN_MEMORY_SIZE_IN_BYTES) && (i<NUM_TEST_ACCESSES);j++) {
      if (rand()%2) { //randomly choose to read or write
	//reading from the word at address+(j*4) 
	memory_access(address + (j<<2), 0, READ_ENABLE_MASK, &read_data);
      }
      else {
	//writing to the word at address+(j*4) 
	memory_access(address + (j<<2), (1<<20) - address, WRITE_ENABLE_MASK, NULL);      
      }
      i++;

      //Generate a clock interrupt (to clear the r bits in L2) every 8K (= 2^13) memory accesses.
      //This will happen when the lowest 13 bits of i are 0.
      //To check, use the binary mask containing 13 ones:  1 1111 1111 1111 = 1FFF hex
      if (!(i&0x1fff)) { //
	memory_handle_clock_interrupt();
      }
      num_memory_accesses++;
    }
  }
  
  printf("In Pass 4, number of memory accesses = %d\n", num_memory_accesses);
  printf("In Pass 4, number of L1 misses = %d\n", num_l1_misses);
  printf("In Pass 4, number of L2 misses = %d\n", num_l2_misses);

  printf("Passed\n");
}


	
