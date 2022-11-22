
/***********************************************************
   This file contains the code for the L1 cache. It is a 
   64KB (2^16 bytes) direct-mapped, write-back cache.

   Since a word is 32 bits (4 bytes) and a cache line is 16 words, 
   there are a total of 64 bytes (which is 2^6 bytes) in a cache
   line. Thus, there is a total of 

       (2^16 bytes)/(2^6 bytes/cache line) = 2^10 cache lines

   in the L1 cache. Therefore, log (2^10) = 10 bits are needed for
   the index into the L1 cache (for selecting a cache line).

   Thus, address is decomposed as follows (from lsb to msb):
   2 bits are used for byte offset within a word (bits 0-1)
   4 bits are used for word offset within a cache line (bits 2-5)
  10 bits are used for the cache index (bits 6-15), see above.
  16 bits remaining are used as the tag (bits 16-31)

  So, the 32-bit address is decomposed as follows:

             16              10           4       2   
    ---------------------------------------------------
   |        tag      |     index     |  word  |  byte  |
   |                 |               | offset | offset |
    ---------------------------------------------------


   Each L1 cache entry is structured as follows:

    1 1    14       16            
   -------------------------------------------------
   |v|d|reserved|  tag  |  16-word cache line data  |
   -------------------------------------------------

  where "v" is the valid bit and "d" is the dirty bit.
  The 14-bit "reserved" field is an artifact of using
  C, it wouldn't be in the actual cache hardware.

************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_subsystem_constants.h"
#include "l1_cache.h"


//number of cache entries (2^11)
#define L1_NUM_CACHE_ENTRIES (1<<11)

/***************************************************
This struct defines the structure of a single cache
entry in the L1 cache. It has the following fields:
  v_d_tag: 32-bit unsigned word containing the 
           valid (v) bit at bit 31 (leftmost bit),
           the dirty bit (d) at bit 30, and the tag 
           in bits 0 through 15 (the 16 rightmost bits)
  cache_line: an array of 16 words, constituting a single
              cache line.
****************************************************/
typedef struct {
  uint32_t v_d_tag;
  uint32_t cache_line[WORDS_PER_CACHE_LINE];
} L1_CACHE_ENTRY;


//the valid bit is bit 31 (leftmost bit) of v_d_tag word
//The mask is 1 shifted left by 31
#define L1_VBIT_MASK (0x1 << 31)

//dirty bit is bit 30 (second to leftmost bit) of v_d_tag word
//The mask is 1 shifted left by 30
#define L1_DIRTYBIT_MASK (0x1 << 30)

//tag is lowest 16 bits of v_d_tag word, so the mask is FFFF hex
#define L1_ENTRY_TAG_MASK 0xffff

//The L1 cache is just an array cache entries
L1_CACHE_ENTRY l1_cache[L1_NUM_CACHE_ENTRIES];


/************************************************
            l1_initialize()

This procedure initializes the L1 cache by clearing
the valid bit of each cache entry.
************************************************/

void l1_initialize()
{
  //just need to zero out the valid bit in each cach entry. 
  //However, there's no reason not to write a 0 to the entire
  //v_d_tag field, since that's more efficient (no masking/shifting)

    for (int entry = 0; entry < L1_NUM_CACHE_ENTRIES; entry++) {
        l1_cache[entry].v_d_tag = 0;
    }
}


//The upper 16 bits (bits 16-31) of an address are used as the tag bits.
//So the mask is 16 ones (so FFFF hex) shifted left by 20 bits.
#define L1_ADDRESS_TAG_MASK (0xffff << 16)
#define L1_ADDRESS_TAG_SHIFT 16

//Bits 6-15 (so 10 bits in total) of an address specifies the index of the
//cache line within the L1 cache.
//The value of the mask is 10 ones (so 3FF hex) shifted left by 6 bits,
//and the shift is 6.
#define L1_ADDRESS_INDEX_MASK (0x3ff << 6)
#define L1_ADDRESS_INDEX_SHIFT 6


//Bits 2-5 of an address specifies the 4-bit offset of the addressed
//word within the 16-word cache line.
// So the mask is 4 ones (so F hex) shifted left by 2.
//
#define L1_ADDRESS_WORD_OFFSET_MASK (0xF << 2)

//After masking to extract the word offset, it needs
//to be shifted to the right by 2.
#define L1_ADDRESS_WORD_OFFSET_SHIFT 2


#define L1_HIT_STATUS_MASK 0x1

/**********************************************************

             l1_cache_access()

This procedure implements the reading or writing of a single word
to the L1 cache. 

The parameters are:

address:  unsigned 32-bit address. This address can be anywhere within
          a cache line.

write_data: a 32-bit word. On a write operation, if there is a cache
          hit, write_data is copied to the appropriate word in the
          appropriate cache line.

control:  an unsigned byte (8 bits), of which only the two lowest bits
          are meaningful, as follows:
          -- bit 0:  read enable (1 means read, 0 means don't read)
          -- bit 1:  write enable (1 means write, 0 means don't write)

read_data: a 32-bit ouyput parameter (thus, a pointer to it is passed).
         On a read operation, if there is a cache hit, the appropriate
         word of the appropriate cache line in the cache is written
         to read_data.

status: this is an 8-bit output parameter (thus, a pointer to it is 
        passed).  The lowest bit of this byte should be set to 
        indicate whether a cache hit occurred or not:
              cache hit: bit 0 of status = 1
              cache miss: bit 0 of status = 0

If the access results in a cache miss, then the only
effect is to set the lowest bit of the status byte to 0.

**********************************************************/


void l1_cache_access(uint32_t address, uint32_t write_data, 
		     uint8_t control, uint32_t *read_data, uint8_t *status)
{

  //Extract from the address the index of the cache entry in the cache.
  //Use the L1_ADDRESS_INDEX_MASK mask to mask out the appropriate
  //bits of the address and L1_ADDRESS_INDEX_SHIFT
  //to shift the appropriate amount.

    uint32_t entry_index = (address & L1_ADDRESS_INDEX_MASK) >> L1_ADDRESS_INDEX_SHIFT;

  //Extract from the address the tag bits.
  //Use the L1_ADDRESS_TAG_MASK mask to mask out the appropriate
  //bits of the address and L1_ADDRESS_TAG_SHIFT
  //to shift the appropriate amount.

    uint32_t tag = (address & L1_ADDRESS_TAG_MASK) >> L1_ADDRESS_TAG_SHIFT;
  
  //Extract from the address the word offset within the cache line.
  //Use the L1_ADDRESS_WORD_OFFSET_MASK to mask out the appropriate bits of
  //the address and L1_ADDRESS_WORD_OFFSET_SHIFT to shift the bits the 
  //appropriate amount.

    uint32_t word_offset = (address & L1_ADDRESS_WORD_OFFSET_MASK) >> L1_ADDRESS_WORD_OFFSET_SHIFT;
  
  //if the cache entry at the computed index has a zero valid bit or 
  //if the entry's tag does not match the tag bits of
  //the address, then it is a cache miss: Set the
  //low bit of the status byte appropriately. There's nothing
  //more to do in this case, the function can return.

    if (((l1_cache[entry_index].v_d_tag & L1_VBIT_MASK) == 0) || ((l1_cache[entry_index].v_d_tag & L1_ENTRY_TAG_MASK) != tag)) {
        *status &= ~(0x1);
        return;
    }

  //Otherwise, it's a cache hit:
  //If a read operation was specified, the appropriate word (as specified by
  //the word offset extracted from the address) of the entry's 
  //cache line data should be written to read_data.
  //If a write operation was specified, the value of write_data should be
  //written to the appropriate word of the entry's cache line data and 
  //the entry's dirty bit should be set.

    *status |= (0x1);
    if (control & READ_ENABLE_MASK) {
        *read_data = l1_cache[entry_index].cache_line[word_offset];
    }
    if (control & WRITE_ENABLE_MASK) {
        l1_cache[entry_index].cache_line[word_offset] = write_data;
        l1_cache[entry_index].v_d_tag |= L1_DIRTYBIT_MASK;
    }
}

/********************************************************

             l1_insert_line()

This procedure inserts a new cache line into the L1 cache.

The parameters are:

address: 32-bit memory address for the new cache line.

write_data: an array of unsigned 32-bit words containing the 
            cache line data to be inserted into the cache.

evicted_writeback_address: a 32-bit output parameter (thus,
          a pointer to it is passed) that, if the cache line
          being evicted needs to be written back to memory,
          should be assigned the memory address for the evicted
          cache line.
          
evicted_writeback_data: an array of 32-bit words that, if the cache 
          line being evicted needs to be written back to memory,
          should be assigned the cache line data for the evicted
          cache line. Since there are 16 words per cache line, the
          actual parameter should be an array of at least 16 words.

status: this in an 8-bit output parameter (thus, a pointer to it is 
        passed).  The lowest bit of this byte should be set to 
        indicate whether the evicted cache line needs to be
        written back to memory or not, as follows:
            0: no write-back required
            1: evicted cache line needs to be written back.

*********************************************************/

void l1_insert_line(uint32_t address, uint32_t write_data[], 
		    uint32_t *evicted_writeback_address, 
		    uint32_t evicted_writeback_data[], 
		    uint8_t *status)
{

  //Extract from the address the index of the cache entry in the cache.
  //See l1_cache_access, above.

    uint32_t entry_index = (address & L1_ADDRESS_INDEX_MASK) >> L1_ADDRESS_INDEX_SHIFT;

  //Extract from the address the tag bits.
  //See l1_cache_access, above.

    uint32_t tag = (address & L1_ADDRESS_TAG_MASK) >> L1_ADDRESS_TAG_SHIFT;

  //If the cache entry at the computed index has the valid bit = 1 and 
  //the dirty bit = 1, then that cache entry has to be written back
  //before being overwritten by the new cache line.  

  //The address to write the current entry back to is constructed from the
  //entry's tag and the index in the cache by:
  // (evicted_entry_tag << L1_ADDRESS_TAG_SHIFT) | (index << L1_ADDRESS_INDEX_SHIFT)
  //This address should be written to the evicted_writeback_address output
  //parameter.

  //The cache line data in the current entry should be copied to the
  //evicted_writeback_data array.

  //The lowest bit of the status byte should be set to 1 to indicate that
  //the write-back is needed.

    if ((l1_cache[entry_index].v_d_tag & L1_VBIT_MASK) && (l1_cache[entry_index].v_d_tag & L1_DIRTYBIT_MASK)) {
        *evicted_writeback_address = (l1_cache[entry_index].v_d_tag << L1_ADDRESS_TAG_SHIFT) | (entry_index << L1_ADDRESS_INDEX_SHIFT);
        for (int i = 0; i < WORDS_PER_CACHE_LINE; i++) {
            evicted_writeback_data[i] = l1_cache[entry_index].cache_line[i];
        }
        *status |= (0x1);
    }

  //Otherwise, i.e. either the current entry is not valid or hasn't been written to,
  //no writeback is needed. Just set the lowest bit of the status byte to 0.

    else{
        *status &= ~(0x1);
    }

  // Now (for both cases, write-back or not), write the incoming cache line
  // in write_data to the selected cache entry.
  
    for (int i = 0; i < WORDS_PER_CACHE_LINE; i++) {
        l1_cache[entry_index].cache_line[i] = write_data[i];
    }
  
  //set the valid bit, clear the dirty bit, and write the tag

  //CODE HERE
    l1_cache[entry_index].v_d_tag = tag;
    l1_cache[entry_index].v_d_tag &= (~(L1_DIRTYBIT_MASK));
    l1_cache[entry_index].v_d_tag |= L1_VBIT_MASK;
} 