
/************************************************************

The L2 cache is a 1MB 4-way set associative, write-back cache.  
As with the rest of the memory subsystem, a cache line is 16 words.

The total (data) size of the L2 cache is 1MB 
                                       = 256K words  (4 bytes/word)
                                       = 16K cache lines (16 words/cache line)
                                       = 4K sets  (4 cache lines/set)


Each cache line has: valid bit, reference bit, dirty bit,
                     tag, and cache-line data.

An address is decomposed as follows (from lsb to msb):
2 bits are used for byte offset within a word (2^2 bytes per cache line), bits 0-1
4 bits are used for word offset within a cache line (2^4 words per cache line), bits 2-5
12 bits are used for the set index, since there are 4K = 2^12 sets per cache, bits 6-17
14 bits remaining are used as the tag (bits 18-31)


          14             13         4        2
    -----------------------------------------------
   |     tag      |    set      |  word  | byte    |
   |              |    index    | offset | offset  |
    -----------------------------------------------


Each cache entry is structured as follows:

    1 1 1     15      14
    ------------------------------------------------
   |v|r|d|reserved|  tag  |  16-word cache line data |
    ------------------------------------------------

where:
  v is the valid bit
  r is the reference bit
  d is the dirty bit

and the 15 "reserved" bits are an artifact of using C. They
would not exist in the cache hardware.

**************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_subsystem_constants.h"
#include "l2_cache.h"


/***************************************************
This struct defines the structure of a single cache
entry in the L2 cache. It has the following fields:
  v_r_d_tag: 32-bit unsigned word containing the:
           valid (v) bit at bit 31 (leftmost bit),
           the reference (r) bit at bit 30,
           the dirty bit (d) at bit 29,
           the tag in bits 0 through 13 (the 14 rightmost bits)
  cache_line: an array of 16 words, constituting a single
              cache line.
****************************************************/

typedef struct {
  uint32_t v_r_d_tag;
  uint32_t cache_line[WORDS_PER_CACHE_LINE];
} L2_CACHE_ENTRY;



/***************************************************
  This structure defines an L2 cache set. Its only
  field, lines, is an array of four cache entries.
***************************************************/

//4-way set-associative cache, so there are
//4 cache lines per set.
#define L2_LINES_PER_SET 4

typedef struct {
  L2_CACHE_ENTRY lines[L2_LINES_PER_SET];
} L2_CACHE_SET;


//There are 4K = 2^12 sets in the L2 cache
#define L2_NUM_CACHE_SETS (1 << 12)

//The l2 cache itself is just an array of 4K = 2^12 cache sets.
L2_CACHE_SET l2_cache[L2_NUM_CACHE_SETS];

//valid bit is bit 31 (leftmost bit) of v_d_tag word
#define L2_VBIT_MASK (1 << 31)

//Mask for r bit: Bit 30 of v_r_d_tag
#define L2_RBIT_MASK (1 << 30)

//dirty bit is bit 29 (second to leftmost bit) of v_d_tag word
#define L2_DIRTYBIT_MASK (1 << 29)

//tag is lowest 14 bits of v_d_tag word.
//the mask is 3FFF hex.
#define L2_ENTRY_TAG_MASK 0x3fff


/**********************************************************
  These constants define how the set index and tag
  fields within an address are accessed.

**********************************************************/


//The upper 14 bits (bits 18-31) of an address are used as the tag bits.
//The mask is 3FFF hex shifted left by 18.
#define L2_ADDRESS_TAG_MASK (0x3fff << 18)
#define L2_ADDRESS_TAG_SHIFT 18

//Bits 6-17 (so 12 bits) of an address specifies the index of the set within
//the L2 cache.
//The value of the mask is FFF hex shifted left by 6.
#define L2_ADDRESS_INDEX_MASK (0xfff << 6)
#define L2_ADDRESS_INDEX_SHIFT 6



//This can be used to set or clear the lowest bit of the status
//register to indicate a cache hit or miss.
#define L2_CACHE_HIT_MASK 0x1



/************************************************
            l2_initialize()

This procedure initializes the L2 cache by clearing
the valid bit of each cache entry in each set in
the cache.
************************************************/

void l2_initialize()
{
  //just need to zero out the valid bit in each cache entry in each set.
  //A zero can be written to the entire v_r_d_tag field, since that's
  //more efficient.

    for (int set = 0; set < L2_NUM_CACHE_SETS; set++) {
        for (int line = 0; line < L2_LINES_PER_SET; line++) {
            l2_cache[set].lines[line].v_r_d_tag = 0;
        }       
    }
}


#define L2_HIT_STATUS_MASK 0x1

/****************************************************

          l2_cache_access()
          
This procedure implements the reading and writing of cache lines from
and to the L2 cache. The parameters are:

address:  unsigned 32-bit address. This address can be anywhere within
          a cache line.

write_data:  an array of unsigned 32-bit words. On a write operation,
             if there is a cache hit, 16 words are copied from write_data 
             to the appropriate cache line in the L2 cache.

control:  an unsigned byte (8 bits), of which only the two lowest bits
          are meaningful, as follows:
          -- bit 0:  read enable (1 means read, 0 means don't read)
          -- bit 1:  write enable (1 means write, 0 means don't write)

read_data: an array of unsigned 32-bit integers. On a read operation,
           if there is a cache hit, 16 32-bit words are copied from the 
           appropriate cache line in the L2 cache to read_data.

status: this in an 8-bit output parameter (thus, a pointer to it is 
        passed).  The lowest bit of this byte should be set to 
        indicate whether a cache hit occurred or not:
              cache hit: bit 0 of status = 1
              cache miss: bit 0 of status = 0

If the access results in a cache miss, then the only
effect is to set the lowest bit of the status byte to 0.

**************************************************/

void l2_cache_access(uint32_t address, uint32_t write_data[], 
		     uint8_t control, uint32_t read_data[], uint8_t *status)
{

  //Extract from the address the index of the cache set in the cache.
  //Use the L2_ADDRESS_INDEX_MASK mask to mask out the appropriate
  //bits of the address and L2_ADDRESS_INDEX_SHIFT to shift the 
  //bits the appropriate amount.

    uint32_t set_index = (address & L2_ADDRESS_INDEX_MASK) >> L2_ADDRESS_INDEX_SHIFT;
  
  //Extract from the address the tag bits.
  //Use the L2_ADDRESS_TAG_MASK mask to mask out the appropriate
  //bits of the address and L2_ADDRESS_TAG_SHIFT to shift the 
  //bits the appropriate amount.

    uint32_t tag = (address & L2_ADDRESS_TAG_MASK) >> L2_ADDRESS_TAG_SHIFT;
  
  //Within the set specified by the set index extracted from the address,
  //look through the cache entries for an entry that 1) has its valid 
  //bit set AND 2) has a tag that matches the tag extracted from the address.
  //If no such entry exists in the set, then the result is a cache miss.
  //The low bit of the status output parameter should be set to 0. There
  //is nothing further to do in this case and the function can return.

  //Otherwise, if an entry is found with a valid bit equal to 1 and a matching tag, 
  //then it is a cache hit. The reference bit of the cache entry should be set
  //and the low bit of status output parameter should be set to 1.

  //If the read-enable bit of the control parameter is 1, then copy
  //the cache line data in the cache entry into read_data.

  //If the write-enable bit of the control parameter is 1, then copy
  //write_data into the cache line data of the cache entry and
  //set the dirty bit.

    int line_index = -1;
    for (int line = 0; line < L2_LINES_PER_SET; line++) {
        if ((l2_cache[set_index].lines[line].v_r_d_tag & L2_VBIT_MASK) &&
            ((l2_cache[set_index].lines[line].v_r_d_tag & L2_ENTRY_TAG_MASK) == tag)){
            line_index = line;
            break;
        }
    }
    if (line_index == -1) {         // cache miss
        *status &= ~(0x1);
    }
    else {      // cache hit
        *status |= (0x1);
        l2_cache[set_index].lines[line_index].v_r_d_tag |= L2_RBIT_MASK;
        if (control & READ_ENABLE_MASK) {
            for (uint32_t i = 0; i < WORDS_PER_CACHE_LINE; i++) {
                read_data[i] = l2_cache[set_index].lines[line_index].cache_line[i];
            }
        }
        if (control & WRITE_ENABLE_MASK) {
            for (uint32_t i = 0; i < WORDS_PER_CACHE_LINE; i++) {
                l2_cache[set_index].lines[line_index].cache_line[i] = write_data[i];
            }
            l2_cache[set_index].lines[line_index].v_r_d_tag |= L2_DIRTYBIT_MASK;
        }
    }
}



/************************************************************

                 l2_insert_line()

This procedure inserts a new cache line into the L2 cache.

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

status: this is an 8-bit output parameter (thus, a pointer to it is 
        passed).  The lowest bit of this byte should be set to 
        indicate whether the evicted cache line needs to be
        written back to memory or not, as follows:
            0: no write-back required
            1: evicted cache line needs to be written back.


 The cache replacement algorithm uses a simple NRU
 algorithm. A cache entry (among the cache entries in the set) is 
 chosen to be written to in the following order of preference:
    - valid bit = 0
    - reference bit = 0 and dirty bit = 0
    - reference bit = 0 and dirty bit = 1
    - reference bit = 1 and dirty bit = 0
    - reference bit = 1 and dirty bit = 1
*********************************************************/

// This constant (which is all 1's) is used to indicate that a cache entry
// with a certain combination of valid, reference, and dirty
// bits has not yet been found in the selected set.

#define NOT_FOUND (~0x0)

void l2_insert_line(uint32_t address, uint32_t write_data[], 
		    uint32_t *evicted_writeback_address, 
		    uint32_t evicted_writeback_data[], 
		    uint8_t *status)
{

  //Extract from the address the index of the cache set in the cache.
  //see l2_cache_access above.

    uint32_t set_index = (address & L2_ADDRESS_INDEX_MASK) >> L2_ADDRESS_INDEX_SHIFT;

  //Extract from the address the tag bits. 
  //see l2_cache_access above.

    uint32_t tag = (address & L2_ADDRESS_TAG_MASK) >> L2_ADDRESS_TAG_SHIFT;

  // The cache replacement algorithm uses a simple NRU
  // algorithm. A cache entry (among the cache entries in the set) is 
  // chosen to be written to in the following order of preference:
  //    - valid bit = 0
  //    - reference bit = 0 and dirty bit = 0
  //    - reference bit = 0 and dirty bit = 1
  //    - reference bit = 1 and dirty bit = 0
  //    - reference bit = 1 and dirty bit = 1
  //  The search loops through the lines in the set. If it 
  //  finds a valid bit = 0, then that entry can be overwritten and
  //  we can exit the loop.
  //  Otherwise, we remember the *first* line we encounter which has r=0 and d=0,
  //  the *first* line that has r=0 and d=1, etc. When we're done looping,
  //  we choose the entry with the highest preference on the above list to evict.

  //This variable is used to store the index within the set 
  //of a cache entry that has its r bit = 0 and its dirty bit = 0.
  //Initialize it to NOT_FOUND to indicate that such a
  //cache entry hasn't been found yet.
  uint32_t r0_d0_index = NOT_FOUND;  

  //This variable is used to store the index within the set 
  //of a cache entry that has its r bit = 0 and its dirty bit = 1.
  uint32_t r0_d1_index = NOT_FOUND;

  //This variable isused to store the index within the set 
  //of a cache entry that has its r bit = 1 and its dirty bit = 0.
  uint32_t r1_d0_index = NOT_FOUND;

  uint32_t word_offset;

  //In a loop, iterate though each entry in the set.

  //LOOP STARTS HERE
  for (int line = 0; line < L2_LINES_PER_SET; line++) {

      // if the current entry has a zero v bit, then overwrite
      // the cache line in the entry with the data in write_data,
      // set the v bit of the entry, clear the dirty and reference bits, 
      // and write the new tag to the entry. Set the low bit of the status
      // output parameter to 0 to indicate the evicted line does not need 
      // to be written back. There is nothing further to do, the
      // function can return.

      if (!(l2_cache[set_index].lines[line].v_r_d_tag & L2_VBIT_MASK)) {
          for (int i = 0; i < WORDS_PER_CACHE_LINE; i++) {
              l2_cache[set_index].lines[line].cache_line[i] = write_data[i];
          }
          l2_cache[set_index].lines[line].v_r_d_tag |= L2_VBIT_MASK;
          l2_cache[set_index].lines[line].v_r_d_tag &= ~L2_DIRTYBIT_MASK;
          l2_cache[set_index].lines[line].v_r_d_tag &= ~L2_RBIT_MASK;
          l2_cache[set_index].lines[line].v_r_d_tag &= ~L2_ENTRY_TAG_MASK;
          l2_cache[set_index].lines[line].v_r_d_tag |= tag;
          *status &= ~(0x1);
          return;
      }
      //  Otherwise, we remember the first entry we encounter which has r=0 and d=0,
      //  the first entry that has r=0 and d=1, etc. When we're done looping,
      //  we choose the entry with the highest preference on the above list to evict.

      else if (!(l2_cache[set_index].lines[line].v_r_d_tag & L2_RBIT_MASK) && !(l2_cache[set_index].lines[line].v_r_d_tag & L2_DIRTYBIT_MASK)) {
          if (r0_d0_index = NOT_FOUND)
              r0_d0_index = line;
      }
      else if (!(l2_cache[set_index].lines[line].v_r_d_tag & L2_RBIT_MASK) && (l2_cache[set_index].lines[line].v_r_d_tag & L2_DIRTYBIT_MASK)) {
          if (r0_d1_index = NOT_FOUND)
              r0_d1_index = line;
      }
      else if ((l2_cache[set_index].lines[line].v_r_d_tag & L2_RBIT_MASK) && !(l2_cache[set_index].lines[line].v_r_d_tag & L2_DIRTYBIT_MASK)) {
          if (r1_d0_index = NOT_FOUND)
              r1_d0_index = line;
      }
      
   
  //LOOP ENDS HERE
    }

  //After the loop, we choose the entry with the highest preference 
  //on the above list to evict. If all the cache entries have
  //v=1, r=1, and d=1, then choose entry 0 of the set to evict.
    
    int line_index = -1;
    if (r0_d0_index != NOT_FOUND) {
        line_index = r0_d0_index;
    }
    else if (r0_d1_index != NOT_FOUND) {
        line_index = r0_d1_index;
    }
    else if (r1_d0_index != NOT_FOUND) {
        line_index = r1_d0_index;
    }
    else {
        line_index = 0;
    }

  //if the dirty bit of the cache entry to be evicted is set, then the data in the 
  //cache line needs to be written back. The address to write the current entry 
  //back to is constructed from the entry's tag and the set index in the cache by:
  // (evicted_entry_tag << L2_ADDRESS_TAG_SHIFT) | (set_index << L2_SET_INDEX_SHIFT)
  //This address should be written to the evicted_writeback_address output
  //parameter. The cache line data in the evicted entry should be copied to the
  //evicted_writeback_data_array.

    if (l2_cache[set_index].lines[line_index].v_r_d_tag & L2_DIRTYBIT_MASK) {
        for (int i = 0; i < WORDS_PER_CACHE_LINE; i++) {
            evicted_writeback_data[i] = l2_cache[set_index].lines[line_index].cache_line[i];
        }
        *evicted_writeback_address = ((l2_cache[set_index].lines[line_index].v_r_d_tag & L2_ENTRY_TAG_MASK) << L2_ADDRESS_TAG_SHIFT) | (set_index << L2_ADDRESS_INDEX_SHIFT);
        
    
  
  //Also, if the dirty bit of the chosen entry is been set, the low bit of the status byte 
  //should be set to 1 to indicate that the write-back is needed. Otherwise,
  //the low bit of the status byte should be set to 0.

        *status |= 0x1;
    }
    else {
        *status &= ~(0x1);
    }

  //Then, copy the data from write_data to the cache line in the entry, 
  //set the valid bit of the entry, clear the dirty bit of the 
  //entry, and write the tag bits of the address into the tag of 
  //the entry.

    for (int i = 0; i < WORDS_PER_CACHE_LINE; i++) {
        l2_cache[set_index].lines[line_index].cache_line[i] = write_data[i];
    }
    l2_cache[set_index].lines[line_index].v_r_d_tag |= L2_VBIT_MASK;
    l2_cache[set_index].lines[line_index].v_r_d_tag &= ~L2_DIRTYBIT_MASK;
    l2_cache[set_index].lines[line_index].v_r_d_tag &= ~L2_RBIT_MASK;
    l2_cache[set_index].lines[line_index].v_r_d_tag &= ~L2_ENTRY_TAG_MASK;
    l2_cache[set_index].lines[line_index].v_r_d_tag |= tag;
}


/************************************************

       l2_clear_r_bits()

This procedure clear the r bit of each entry in each set of the L2
cache. It is called periodically to support the the NRU algorithm.

***********************************************/
    
void l2_clear_r_bits()
{
    for (int set = 0; set < L2_NUM_CACHE_SETS; set++) {
        for (int line = 0; line < L2_LINES_PER_SET; line++) {
            l2_cache[set].lines[line].v_r_d_tag &= ~L2_RBIT_MASK;
        }
    }
}