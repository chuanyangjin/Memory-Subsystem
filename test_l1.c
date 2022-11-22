
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_subsystem_constants.h"
#include "l1_cache.h"


#define L1_HIT_STATUS_MASK 0x1

// L1 cache size is 64KB (2^16 bytes)
#define L1_CACHE_SIZE_IN_BYTES (1 << 16)

int main()
{

  printf("Initializing L1\n");
  l1_initialize();


  uint32_t read_data;
  uint32_t write_data;
  uint32_t new_line[WORDS_PER_CACHE_LINE];
  uint32_t evicted_writeback_data[WORDS_PER_CACHE_LINE];
  uint32_t evicted_writeback_address;

  uint8_t status;

  uint32_t i,j;

  //write to each word in the L1 cache using
  //l1_cache_access(). If there's a cache miss (as 
  //there should be initially), call l1_insert_line

  printf("Pass 1: Writing to each entry of empty L1 cache\n");

  // i is a byte address iterating over each word in the cache (so incremented by BYTES_PER_WORD)

  for(i=0; i < L1_CACHE_SIZE_IN_BYTES; i += BYTES_PER_WORD) {

    //writing the value i*2 at address i
    l1_cache_access(i, i<<1, WRITE_ENABLE_MASK, NULL, &status);

    if (!(status & L1_HIT_STATUS_MASK)) { 

      //cache miss, should only happen on the first word of a cache line
      //i should be divisible by 64 (because 14 words/line, 4 bytes/word)

      if (i & 0x3F) { //not divisible by 64
        printf("Error: Cache misses should only occur for the first word of a line in Pass 1\n");
        exit(1);
      }

      //populate the first word of the cache line with i*2, the rest with phony data, and insert the cache line

      new_line[0] = i<<1;

      for(j=1; j<WORDS_PER_CACHE_LINE;j++) {
        new_line[j] = 1000 + j;
      }

      l1_insert_line(i, new_line, &evicted_writeback_address, 
                     evicted_writeback_data, &status);

      if (status & 0x1) { //if writeback line was evicted, indicated by lsb of status = 1
        //something was wrong.
        printf("Error: No cache line to evict and write back in Pass 1\n");
        exit(1);
      }
    }
    else {  //cache hit, which should only happen when i is not divisible by 64
      if (!(i & 0x3F)) { //divisible by 64
        printf("Error: Cache hits should not occur for first word of a line in Pass 1\n");
        exit(1);
      }
    }
  }


  printf("Pass 2: Reading back the values written in Pass 1\n");

  for(i=0; i < L1_CACHE_SIZE_IN_BYTES; i+=BYTES_PER_WORD) {
    //read the value at address i.
    l1_cache_access(i, ~0x0, READ_ENABLE_MASK, &read_data, &status);

    if (!(status & L1_HIT_STATUS_MASK)) { //cache miss, shouldn't happen
      printf("Error: Cache miss, shouldn't occur in Pass 2\n");
      exit(1);
    }    
    if (read_data != (i<<1)) {  //didn't read back the right value
      printf("Error: Data read back in Pass 2 didn't match the values written in Pass 1\n");
      exit(1);
    }
  }


  printf("Pass 3: Writing to an entirely new set of cache lines (i.e. not already in L1)\n");

  
  for(i = L1_CACHE_SIZE_IN_BYTES; i < (2 * L1_CACHE_SIZE_IN_BYTES); i += BYTES_PER_WORD) {

    l1_cache_access(i, i << 1, WRITE_ENABLE_MASK, NULL, &status);

    if (!(status & 0x1)) { //if cache miss, indicated by lsb of status = 0

      // a cache miss shouldn't happen if i is not divisble by 64
      if (i & 0x3f) {    // not divisible by 64
        printf("Error: Cache miss should only occur in pass 3 on first word in cache line, when i is divisible by 64\n");
        exit(1);
      }
      
      //insert a new line into the cache
      for(j=0; j<WORDS_PER_CACHE_LINE;j++) {
        new_line[j] = i + j;
      }

      //call l1_insert_line
      l1_insert_line(i, new_line, &evicted_writeback_address, 
                     evicted_writeback_data, &status);


      if (!(status & 0x1)) { //if no writeback line was evicted, indicated by lsb of status = 0
        //something was wrong.
        printf("Error: A cache line should be evicted in Pass 3\n");
        exit(1);
      }
    }
    else {  //cache hit, which shouldn't happen if i is divisible by 64
      if (!(i & 0x3f)) {   // i is divisible by 64
        printf("Error: No cache hits should happen in Pass 3 when i is divisible by 64\n");
        exit(1);
      }
    }
  }


  printf("Pass 4: Writing to cache lines already resident in L1 cache\n");

  for(i = L1_CACHE_SIZE_IN_BYTES; i < (2 * L1_CACHE_SIZE_IN_BYTES); i += BYTES_PER_WORD) {

    // writing the value i*2 to address i, so consecutive words -- having a difference of
    // 4 in their address -- will differ by 8.
    l1_cache_access(i, i << 1, WRITE_ENABLE_MASK, NULL, &status);

    if (!(status & 0x1)) { //if cache miss, indicated by lsb of status = 0
      printf("Error: Cache miss in Pass 4 \n");
      exit(1);
    }
  }

  printf("Pass 5: Reading from cache lines written in Pass 1, should miss every time\n");

  //Now reading using addresses from Pass 1, should be a cache miss every time a new
  //line is accessed (i.e. when i is divisble by 64)
  for(i=0; i < L1_CACHE_SIZE_IN_BYTES; i += BYTES_PER_WORD) {

    l1_cache_access(i, ~0x0, READ_ENABLE_MASK, &read_data, &status);

    if (status & 0x1) { //if cache hit, indicated by lsb of status = 1
      if (!(i & 0x3f)) {  // if i is divisble by 64, should not be a cache hit
        printf("Error: Cache hit upon read in Pass 5 when i is divisible by 64\n");
        exit(1);
      }
    }
  }

  printf("Pass 6: Reading from cache lines written in Pass 4, so already in the L1 cache\n");

  for(i = L1_CACHE_SIZE_IN_BYTES; i < (2 * L1_CACHE_SIZE_IN_BYTES); i += BYTES_PER_WORD) {

    l1_cache_access(i, ~0x0, READ_ENABLE_MASK, &read_data, &status);

    if (!(status & 0x1)) { //if cache miss, indicated by lsb of status = 0
      printf("Error: Cache miss in Pass 6\n");
      exit(1);
    }
    
    if (read_data != (i << 1)) {
      printf("Error: Data read in Pass 6 is different from that written in Pass 4\n: %u\n",
             read_data);
      exit(1);
    }
  }

  printf("Pass 7: Repeating pass 1 to write to cache lines that are not in L1 and then,\n");
  printf("        upon each miss, calling l1_insert_line. Each line evicted should be\n");
  printf("        written back\n");

  for(i=0; i < L1_CACHE_SIZE_IN_BYTES; i += BYTES_PER_WORD) {

    l1_cache_access(i, write_data, WRITE_ENABLE_MASK, NULL, &status);

    if (!(status & 0x1)) { //if cache miss, indicated by lsb of status = 0

      for(j=0; j < WORDS_PER_CACHE_LINE; j++) 
        new_line[j] = i+j;
      
      //call l1_insert_line
      l1_insert_line(i, new_line, &evicted_writeback_address, evicted_writeback_data, &status);
      
      if (!(status & 0x1)) { //if writeback line was not evicted, indicated by lsb of status = 0
        //something was wrong.
        printf("Error: Each evicted cache line should be written back in Pass 7, but wasn't\n");
        exit(1);
      }

      // Check the write-back address and write-back data. It should be the addresses
      // inserted into the cache in pass 3 and the data written in pass 4, namely:
      //     - the write-back address should be i + L1_CACHE_SIZE_IN_BYTES
      //     - the write-back data should (L1_CACHE_SIZE_IN_BYTES + i)*2,
      //       (L1_CACHE_SIZE_DATA + i + 4)*2, ...,
      //       (L1_CACHE_SIZE_DATA + i + (7*4))*2.

      if (evicted_writeback_address != (i + L1_CACHE_SIZE_IN_BYTES)) {
        printf("Write-back address is incorrect\n");
        exit(1);
      }

      for(j = 0; j < WORDS_PER_CACHE_LINE; j++) {
        if (evicted_writeback_data[j] != (L1_CACHE_SIZE_IN_BYTES + i + (j*4)) << 1) {
          printf("Write-back data is incorrect\n");
	  printf("When i = %u and j = %u, write_back[j] = %u\n", i, j, evicted_writeback_data[j]);
	  printf("It should be %u\n", (L1_CACHE_SIZE_IN_BYTES + i + (j*4)) << 1);
          exit(1);
        }
      }
      
    }
    else {  //cache hit, which shouldn't happen when i is divisble by 64
      if (!(i & 0x3f)) {  // if i is divisble by 64, should be a cache hit      
        printf("Error: No cache line hits should happen in Pass 7\n");
        exit(1);
      }
    }
  }
  
  printf("Passed\n");
}

