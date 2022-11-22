

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <math.h>
#include <time.h>

#include "memory_subsystem_constants.h"
#include "l2_cache.h"

// There are 32K = 2^14 cache lines (see l2_cache.c)
#define L2_NUM_CACHE_LINES (1 << 14)

int main()
{
  
  l2_initialize();

  //Pass 1: Filling all lines of the cache with data

  uint32_t read_data[WORDS_PER_CACHE_LINE];
  uint32_t write_data[WORDS_PER_CACHE_LINE];
  uint32_t evicted_writeback_data[WORDS_PER_CACHE_LINE];
  uint32_t evicted_writeback_address;

  uint8_t status;

  uint32_t i,j;

  printf("Pass 1: Writing to each entry of empty L2 cache\n");

  //i is the byte address of the lowest word in the cache line

  for(i=0; i < (L2_NUM_CACHE_LINES * BYTES_PER_CACHE_LINE); i+=BYTES_PER_CACHE_LINE) {
    // fill write_data with data
    for(j=0;j<WORDS_PER_CACHE_LINE;j++) {
      write_data[j] = i+j;
    }
    //attempting to write write_data at address i,but should result in a cache miss
    l2_cache_access(i, write_data, WRITE_ENABLE_MASK, NULL, &status);

    if (status & 0x1) {   // if cache hit
      printf("Error: Cache hit should not happen in Pass 1, address = %x\n", i);
      exit(1);
    }

    // cache miss, so proceeding.  Insert write_data as a cache line

    l2_insert_line(i, write_data, &evicted_writeback_address, 
		   evicted_writeback_data, &status);

    if (status & 0x1) { //if writeback line was evicted, indicated by lsb of status = 1
      //something was wrong.
      printf("Error: No cache line to evict and write back in Pass 1\n");
      exit(1);
    }
  }

  printf("Pass 2: Reading back the values written in Pass 1\n");

  for(i=0; i < (L2_NUM_CACHE_LINES * BYTES_PER_CACHE_LINE); i+=BYTES_PER_CACHE_LINE) {
    //read the value at address i.
    l2_cache_access(i, NULL, READ_ENABLE_MASK, read_data, &status);

    if (!(status & 0x1)) { //cache miss, shouldn't happen
      printf("Error: Cache miss, shouldn't occur in Pass 2\n");
      exit(1);
    }

    // check if the data read is the same as the data writtten
    for(j=0;j<WORDS_PER_CACHE_LINE;j++) {
      if(read_data[j] != i+j) {
	printf("Error: In pass 2, data read from L2 cache didn't match data written to cache\n");
	exit(1);
      }
    }
  }


  printf("Pass 3: Generating reads at random addresses, cache misses\n");
  printf("        should occur only for addresses greater than or equal to 2^20 (past 1MB)\n");

  //  srand(time(NULL));
  srand(2468);   // CHANGE THIS BACK TO time(NULL)

  uint32_t address;

  for(i=0;i<(1<<22);i++) {
    address = (rand()%(1<<22)) & ~0x3; //generate a random number between 0 and 2^22, divisible by 4
    l2_cache_access(address, NULL, READ_ENABLE_MASK, read_data, &status);

    //cache miss should only happen if address >= 1^20
    if ((!(status & 0x1)) && (address < (1 << 20))) {
	printf("Error: Cache miss on address %u, shouldn't occur in Pass 2\n", address);
	exit(1);
      }

    //cache hit should only happen if address < 1^20
    if ((status & 0x1) && (address >= (1 << 20))) {
	printf("Error: Cache hit on address %u, shouldn't occur in Pass 2\n", address);
	exit(1);
      }
  }

  printf("Pass 4: Testing cache replacement policy.\n");

  l2_initialize();  //need to start with a clean cache

  //for each of the 4K = 2^12 sets in the L2 cache

  uint32_t setnum;

#define LINES_PER_SET 4
#define L2_SETS_PER_CACHE (1 << 12)
#define L2_ADDRESS_INDEX_SHIFT 6  
#define L2_ADDRESS_TAG_SHIFT 18

  for (setnum=0; setnum<L2_SETS_PER_CACHE;setnum++) {

    //insert 4 lines into that set, calling them r0d0,r0d1,r1d0,r1d1
    //we do so by putting setnum in the index field and a different
    // number in each tag field.
    uint32_t r0d0 = (setnum << L2_ADDRESS_INDEX_SHIFT) + (0<<L2_ADDRESS_TAG_SHIFT);
    uint32_t r0d1 = (setnum << L2_ADDRESS_INDEX_SHIFT) + (1<<L2_ADDRESS_TAG_SHIFT);
    uint32_t r1d0 = (setnum << L2_ADDRESS_INDEX_SHIFT) + (2<<L2_ADDRESS_TAG_SHIFT);
    uint32_t r1d1 = (setnum << L2_ADDRESS_INDEX_SHIFT) + (3<<L2_ADDRESS_TAG_SHIFT);


    for(j=0;j<WORDS_PER_CACHE_LINE;j++)
      write_data[j] = setnum + j;

    l2_insert_line(r0d1, write_data, &evicted_writeback_address, 
		   evicted_writeback_data, &status);

    if (status & 0x1) { //writeback, shouldn't occur here
      printf("Error: Writeback occurred when r0d1, address %u, was inserted\n", r0d1);
      exit(1);
    }

    l2_insert_line(r0d0, write_data, &evicted_writeback_address, 
		   evicted_writeback_data, &status);

    if (status & 0x1) { //writeback, shouldn't occur here
      printf("Error: Writeback occurred when r0d0, address %u, was inserted\n", r0d0);
      exit(1);
    }

    l2_insert_line(r1d0, write_data, &evicted_writeback_address, 
		   evicted_writeback_data, &status);

    if (status & 0x1) { //writeback, shouldn't occur here
      printf("Error: Writeback occurred when r1d0, address %u, was inserted\n", r1d0);
      exit(1);
    }

    l2_insert_line(r1d1, write_data, &evicted_writeback_address, 
		   evicted_writeback_data, &status);

    if (status & 0x1) { //writeback, shouldn't occur here
      printf("Error: Writeback occurred when r1d1, address %u, was inserted\n", r1d1);
      exit(1);
    }


    // give a different value for the words in write_data
    for(j=0;j<WORDS_PER_CACHE_LINE;j++)
      write_data[j] <<= 1;

  //write to r0d1 

    l2_cache_access(r0d1, write_data, WRITE_ENABLE_MASK, NULL, &status);

    if (!(status & 0x1)) {
      printf("Error: Cache miss on writing to r0d1\n");
      exit(1);
    }

  //clear r bits


    l2_clear_r_bits();


  //write to r1d1

    l2_cache_access(r1d1, write_data, WRITE_ENABLE_MASK, NULL, &status);

    if (!(status & 0x1)) {
      printf("Error: Cache miss on writing to r1d1\n");
      exit(1);
    }


  //read from r1d0

    l2_cache_access(r1d0, NULL, READ_ENABLE_MASK, read_data, &status);

    if (!(status & 0x1)) {
      printf("Error: Cache miss on reading from to r1d0\n");
      exit(1);
    }


    // insert line new1. Check that there is no writeback (since r0d0 should be evicted).

#define L2_BYTES_PER_CACHE (L2_SETS_PER_CACHE * LINES_PER_SET * BYTES_PER_CACHE_LINE)

    uint32_t new1 = r0d0 + L2_BYTES_PER_CACHE;

    l2_insert_line(new1, write_data, &evicted_writeback_address, 
		   evicted_writeback_data, &status);

    if (status & 0x1) { //error, writeback indicated.
      printf("Error: r0d0, address %u, should not have to be written back\n", r0d0);
      exit(1);
    }

  //write to new1 so it won't be evicted.

    l2_cache_access(new1, write_data, WRITE_ENABLE_MASK, NULL, &status);

  //insert line new2. Check that r0d1 is evicted, writeback.

    uint32_t new2 = r0d1 + L2_BYTES_PER_CACHE;

    l2_insert_line(new2, write_data, &evicted_writeback_address, 
		   evicted_writeback_data, &status);

    if (evicted_writeback_address != r0d1) {
      printf("Error: r0d1, address %u, should have been evicted rather than address %u\n", r0d1,
	     evicted_writeback_address);
      exit(1);
    }

    if (!(status & 0x1)) { //error, writeback not indicated.
      printf("Error: r0d, address %u, should be written back\n", r0d1);
      exit(1);
    }


  //write to new2 so it won't be evicted

    l2_cache_access(new2, write_data, WRITE_ENABLE_MASK, NULL, &status);


    // insert line new3. Check that there is no writeback (since r1d0 should be evicted).

    uint32_t new3 = r1d0 + L2_BYTES_PER_CACHE;

    l2_insert_line(new3, write_data, &evicted_writeback_address, 
		   evicted_writeback_data, &status);

    if (status & 0x1) { //error, writeback indicated.
      printf("Error: r1d0, address %u, should be written back\n", r1d0);
      exit(1);
    }

  //write to new3 so it won't be evicted

    l2_cache_access(new2, write_data, WRITE_ENABLE_MASK, NULL, &status);

  }

  printf("Passed\n");

}
