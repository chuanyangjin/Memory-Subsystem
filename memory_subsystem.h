

/*******************************************************

        memory_subsystem_initialize()

This procedure is used to initialize the memory subsystem.
It takes a single 32-bit parameter, memory_size_in_bytes,
that determines large the main memory will be.

*******************************************************/

void memory_subsystem_initialize(uint32_t size_in_bytes);


/*****************************************************

              memory_access()

This is the procedure for reading and writing one word
of data from and to the memory subsystem. 

It takes the following parameters:

address:  32-bit address of the data being read or written.

write_data: In the case of a memory write, the 32-bit value
          being written.

control:  an unsigned byte (8 bits), of which only the two lowest bits
          are meaningful, as follows:
          -- bit 0:  read enable (1 means read, 0 means don't read)
          -- bit 1:  write enable (1 means write, 0 means don't write)

read_data: a 32-bit ouput parameter (thus, a pointer to it is passed).
         In the case of a read operation, the data being read will
         be written to read_data.

****************************************************/

void memory_access(uint32_t address, uint32_t write_data,
		   uint8_t control, uint32_t *read_data);



/****************************************************

     memory_handle_clock_interrupt

This procedure should be called periodically (e.g. when a clock 
interrupt occurs) in order to cause the r bits in the 
L1 cache to be clear in support of the NRU replacement algorithm.

*******************************************************/

void memory_handle_clock_interrupt();
 
