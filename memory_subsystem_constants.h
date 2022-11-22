#define BOOL int
#define TRUE 1
#define FALSE 0



//There are 4 bytes per word, 16 words per cache line,
//so 64 bytes per cache line.

#define BYTES_PER_WORD 4
#define WORDS_PER_CACHE_LINE 16
#define BYTES_PER_CACHE_LINE 64

//In the 1-byte control line specifying reading and/or
//writing to cache or main memory, bit 0 specifies 
//whether to perform a read, bit 1 specifies whether
//to perform a write. These two masks are convenient
//for setting or clearing those bits.

#define READ_ENABLE_MASK 0x1
#define WRITE_ENABLE_MASK 0x2

