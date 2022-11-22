

/************************************************************************
                 main_memory_initialize
This procedure allocates main memory, according to the size specified in bytes.
The procedure should check to make sure that the size is a multiple of 64 (since
there are 4 bytes per word and 16 words per cache line.
*************************************************************************/

void main_memory_initialize(uint32_t size_in_bytes);

/********************************************************************
               main_memory_access

This procedure implements the reading and writing of cache lines from
and to main memory. The parameters are:

address:  unsigned 32-bit address. This address can be anwhere within
          a cache line.

write_data:  an array of unsigned 32-bit words. On a write operation,
             16 words are copied from write_data to the appropriate cache
             line in memory.

control:  an unsigned byte (8 bits), of which only the two lowest bits
          are meaningful, as follows:
          -- bit 0:  read enable (1 means read, 0 means don't read)
          -- bit 1:  write enable (1 means write, 0 means don't write)

read_data: an array of unsigned 32-bit integers. On a write operation,
           16 32-bit words are copied from the appropriate cache line in 
           memory to read_data.

*********************************************************/

void main_memory_access(uint32_t address, uint32_t write_data[], 
			uint8_t control, uint32_t read_data[]);


