/*********************************************************************
 * Author:	Sergio Delgado
 * G#:		00512529
 * Course:	CS 367-001
 * Project: 3
*********************************************************************/


/*********************************************************************
  		             	   KNOWN PROBLEMS

 1. Least Recently Accessed status:  This problem was solved by 
	implementing a priority queue (an array of integers), which would
	store the indexes of the entries of a set, sorted by the order
	in which they have been used (from the least recently used queue[0]
	to the most recently queue[n].  Every time an entry was accessed it
	was sent to the end of the array.  Every time the least recently
	entry was requiered, I would use the value stored at queue[0] to 
	index the least recently accessed entry.

 2. Extracting a Memory Page from Main Memory:  This problem was solved
	by having a helper function that would receive the Physical Address
	and then: a. calculate the address of the begining of the Memory Page
	through the operation (Address & ~0xf),  b. read 4 consecutive integers
	with the assistance of the get_word(pa),  c. read every byte from 
	that integer (in little endian format) and store every read byte
	into an array  (unsigned char dataBlock[16]).  This dataBlock is 
	what would be stored in an entry of the cache memory, and this would
	make it really simple to extract a specific byte from the data page
	with a specific offset.

 3. Data Structure of TLB and cache:  The decision was made to use an
	array of Structs for each table.  The main element of the TLB array
	is of (type_tlbSet) and the main element of the cache array is of 
	(type_cacheSet).  Each cache and TLB set contains, as one of its
	fields, a corresponding array of (cacheEntries) or (tlbEntries). 

 4. Data Structure of Page Table:  This table was implemented with an
	array of int values. Each entry of the table was stored in an 
	integer, and the fields (valid bit, and PPN) where stored and 
	retrieved at a binary level (bits from 0 to 9 to store PPN, and bit
	10 to store the valid bit).

 5. Access to the Tables:  Since the tables needed to be accessed by a 
	numerous functions in this project, the decision was made to declare
	this tables as "static global".  This would allow the access and 
	updating of tables at any time, and also make sure that the data would
	remain stored on the tables during the whole execution of the program.
	The decision of making them static variables is intended to prevent 
	any inconcistencies during linking time, in case there happened to be
	global variables in files of this project that I was not allowed to 
	modify.

*********************************************************************/



#include <stdio.h>
#include <stdlib.h>
#include "memory_system.h"

#define TLB_SIZE 0x400  //  2^10
#define CACHE_SIZE 0x20  //  2^5
#define PT_SIZE 0x40000	  //  2^18
#define PAGE_SIZE 0x10	// 2^4
#define MAX_TLB_ENTRIES 0x4  // 2^2
#define MAX_CACHE_ENTRIES 0x2 // 2^1
#define TRUE 1
#define FALSE 0


/******************** DEFINITION OF STUCTURES ************************/
typedef struct {
	char valid;
	int tag;
	int ppn;
} type_tlbEntry;

typedef struct {
	int leastRecent[MAX_TLB_ENTRIES];
	type_tlbEntry tlbEntries[MAX_TLB_ENTRIES];
} type_tlbSet;

typedef struct {
	char valid;
	int tag;
	unsigned char data[PAGE_SIZE];
} type_cacheEntry;

typedef struct {
	int leastRecent[MAX_CACHE_ENTRIES];
	type_cacheEntry cacheEntries[MAX_CACHE_ENTRIES];
} type_cacheSet;





/************************ PROTOTYPES ********************************/
void getDataBlock(int, unsigned char[]);




/***** DECLARATION OF TABLE:  TLB, PT, cache (for global use) *******/
static type_tlbSet TLB[TLB_SIZE];
static int PT[PT_SIZE];
static type_cacheSet cache[CACHE_SIZE];




/***********************************************************************
 *
 *	This function takes the Virtual Address and extracts every one of the 
 *	components it stores:  vpn, tlb tab, tlb index, vpo
 *
************************************************************************/
int deconstructVA(int va, int *vpn, int *tlbTag, int *tlbIndex, int *vpo)
{
	int mask;
	int temp = 0;
	
	if (va < 0 || va > 0xffffff)
		return FALSE;

	// Extracting vpo  (6 bits)
	
	mask = 0x3f;   //  binary  0011 1111
	temp = va & mask;  // extracting 6 bits
	*vpo = temp;	
	// Extracting vpn  (18 bits)
	
	va = va >> 6;   // discarding 6 right most bits
	mask = 0x3ffff;  // binary 0011 1111 1111 1111 1111
	temp = va & mask;  // extracting 18 bits
	*vpn = temp;
	
	// Extracting tlb index

	mask = 0x3ff;	// binary 0011 1111 1111
	temp  = va & mask; // extracting 10 bits
	*tlbIndex = temp;

	// Extracting tlb tag
	
	va = va >> 10;   // discarding 10 right most bits
	mask = 0xff;
	temp = va & mask;	// extrancing 8 bits
	*tlbTag = temp;

	return TRUE;
}


/***********************************************************************
 *
 *	This function takes the Physical Address and extracts every one of the 
 *	components it stores:  cacheTag, cacheIndex, cacheOffset
 *
************************************************************************/
int deconstructPA(int pa, int *cacheTag, int *cacheIndex, int *cacheOffset)
{
	int mask;
	int temp = 0;
	
	if (pa < 0 || pa > 0xffff)
		return FALSE;

	// Extracting cacheOffset  (4 bits)
	
	mask = 0xf;   //  binary  0000 1111
	temp = pa & mask;  // extracting 4 bits
	*cacheOffset = temp;	

	// Extracting cacheIndex  (5 bits)
	
	pa = pa >> 4;   // discarding 4 right most bits
	mask = 0x1f;  // binary 0001 1111
	temp = pa & mask;  // extracting 5  bits
	*cacheIndex = temp;
	
	// Extracting cacheTag index
	
	pa = pa >> 5;
	mask = 0x7f;	// binary 0111 1111
	temp  = pa & mask; // extracting 7 bits
	*cacheTag = temp;

	return TRUE;
}


/*******************************************************************************
 *
 *	Function that returns true if a page table entry is valid.
 *
 * ****************************************************************************/
int isPtEntryValid(int ptEntry)
{
	int mask = 1;
	mask = mask << 10;
	
	if ((ptEntry & mask) == mask)
		return TRUE;
	else
		return FALSE;
		
}



/******************************************************************************
 *  Function that switches the bit of an integer value from 0 to 1 or from 1 to 0
 *  specified by parameter bitValue.  The parater bitPosition will specify bit 
 *  index from least significant (0) to any other valid index.
 *  
 *  **************************************************************************/
void switchBit(int *number, int bitPosition, int bitValue)
{
	int mask = 1;
	mask = mask << bitPosition;

	if (bitValue == 0)	// switches that bit to 0, regardless of its original value
		*number = *number & ~mask;


	else				// switches that bit to 1, regardless of its original value
		*number = *number | mask;
	
	return;
}



/******************************************************************************
 *  Function that forms the Physical Address from receiving a PPN and PPO
 *
 ** **************************************************************************/
int constructPA( int ppn, int ppo)
{
	int PA = 0;

	PA = ppo;
	ppn = ppn << 6;
	PA += ppn;

	return PA;
}


/*******************************************************************************
*
* Function that pushes the number of the most recently used entry to the back
* of the queue. The queue keeps the order of entries (from least recently used
* to most recently used.
*
*****************************************************************************/
void makeMostRecent(int *queue, int mostRecentEntry)
{
	int k=0;
	while (queue[k] != mostRecentEntry)
		k++;

	for (int i = k; i<3; i++)
		queue[i] = queue[i+1];

	queue[3] = mostRecentEntry;	

	return;
}



/**************************************************************************
*
* Function that Updates the TLB when there was a TLB miss and a ppn had
* to be obtained from the Page Table
*
* *************************************************************************/
void updateTLB( int tlbIndex, int tlbTag, int newPpn)
{
	//    checking if for the first invalid TLB entry
	int available = -1;
	int i = 0;
	
	while (available == -1 && i<4)	// looking for available invalid entry
	{
		if (TLB[tlbIndex].tlbEntries[i].valid == 0)
			available = i;
		i++;
	}
	
	if (available == -1)		// if there was no invalid entry available
		available = TLB[tlbIndex].leastRecent[0];	// get least recently used entry

	//     updating the least recently accessed entry 
	TLB[tlbIndex].tlbEntries[available].valid = 1;
	TLB[tlbIndex].tlbEntries[available].tag = tlbTag;
	TLB[tlbIndex].tlbEntries[available].ppn = newPpn;

	makeMostRecent( TLB[tlbIndex].leastRecent, available); // resorting least
																	// recently accessed queue
	return;
}



/**************************************************************************
*
* Function that updates the Page Table when a new PPN was obtained from 
* the handle_page_fault function
*
**************************************************************************/
void updatePT( int vpn, int ppn)
{
	switchBit(&ppn, 10, 1);		// switching the valid bit on Page Table entry to 1.
	
	PT[vpn] = ppn;					// updating the entry of Page Table to new ppn

	return;
}



/******************************************************************************
*
*	Function that returns the PPN fron the Page Table, given a virtual address
*	(va) and it returns -1	if the PPN was not found in the Page Table
*
******************************************************************************/
int getFromPageTable( int vpn)
{
	int ppn = -1;

	if (isPtEntryValid( PT[vpn]) )// if Page Table entry (indexed by vpn) is valid 
	{
		ppn = PT[vpn];					// getting a copy of the Page table entry
		switchBit(&ppn, 10, 0);		// disposing of the valid bit in entry(set to 0)
	}

	return ppn;
}




/*******************************************************************************
*
*	Function that returns the PPN from the TLB, given a virtual address (va) and
*	returns -1	if the PPN was not in the TLB 
*
* ****************************************************************************/
int getFromTLB( int tlbTag, int tlbIndex)
{
	int indexOfHit = 0;
	int TLBHit = FALSE;
	int ppn = -1;

	while ( !TLBHit && indexOfHit < 4)// parsing through all entries of indexed set
	{
		if ( (TLB[tlbIndex].tlbEntries[indexOfHit].valid == 1) && 
				(TLB[tlbIndex].tlbEntries[indexOfHit].tag == tlbTag) ) 	
		{	
			TLBHit = TRUE;		// it there is a TLB hit we set TLB Hit flag to TRUE
			indexOfHit--;
		}

		indexOfHit++;
	}
	
	if (TLBHit)		// if there was a hit
	{
		ppn = TLB[tlbIndex].tlbEntries[indexOfHit].ppn;	// we retrieve the ppn
		makeMostRecent( TLB[tlbIndex].leastRecent, indexOfHit); // re-sort
																					// leastRecent Queue
	}

	return ppn;
}



/**************************************************************
 *
 * Helper function to print data from a TLB Set
 *
 * ************************************************************/
void printTlbSet (int i)
{

	printf("TLB index: %X\n", i);
	for (int j=0; j<4; j++)
	{
		printf("ENTRY %X,  Valid: %X,  Tag: %X,  Ppn: %X\n",
					j,
					TLB[i].tlbEntries[j].valid,
					TLB[i].tlbEntries[j].tag,
					TLB[i].tlbEntries[j].ppn);
		
	}
	printf("{");
	for(int j=0; j<4; j++)
		printf("%d ", TLB[i].leastRecent[j]);
	printf("}\n");
}




/************************************************************************
 *
 *	Function in charge of building and initializing every entry of every
 *	one of the 3 tables (TLB, Page Table, cache) to all 0's.
 *
 * **********************************************************************/
void initialize()
{
	// Initializing TLB
	
	for (int i=0; i<TLB_SIZE; i++)
	{
		for (int j=0; j<MAX_TLB_ENTRIES; j++)
		{
			TLB[i].leastRecent[j] = j;
			TLB[i].tlbEntries[j].valid = 0;
			TLB[i].tlbEntries[j].tag = 0;
			TLB[i].tlbEntries[j].ppn = 0;
		}
	}

	// Initializing cache

	for (int i=0; i<CACHE_SIZE; i++)
	{
		for (int j=0; j<MAX_CACHE_ENTRIES; j++)
		{
			cache[i].leastRecent[j] = j;
			cache[i].cacheEntries[j].valid = 0;
			cache[i].cacheEntries[j].tag = 0;

			for (int k=0; k<PAGE_SIZE; k++)
				cache[i].cacheEntries[j].data[k] = 0;
		}
	}

	// Initializing Page Table (PT)
	
	for (int i=0; i<PT_SIZE; i++)
		PT[i] = 0;


 	return;					
}



/*******************************************************************************
*	This is called when the program is exiting.
*  If you want any code to run at program exit, add it in here.
********************************************************************************/
void teardown() 
{
}



/********************************************************************************
*
*	Function that gets a virtual address and returns a physical address can be 
*	by further used to fetch a byte of data from cache or from main memory.
*
********************************************************************************/
int get_physical_address(int va) 
{	

	int pa;
	int ppn; 
	int vpn, tlbTag, tlbIndex, vpo;

	if (va < 0 || va > 0xffffff )		// if virtual address is invalid
	{
		log_entry(ILLEGALVIRTUAL, va);
		return -1;
	}	

	deconstructVA( va, &vpn, &tlbTag, &tlbIndex, &vpo);	// get components from VA

	ppn = getFromTLB( tlbTag, tlbIndex);

	if (ppn != -1)
	{
		pa = constructPA( ppn, vpo);
		log_entry(ADDRESS_FROM_TLB, pa);
		return pa;
	}
			
	ppn = getFromPageTable(vpn);	

	if (ppn != -1)
	{
		updateTLB( tlbIndex, tlbTag, ppn);
		pa = constructPA( ppn, vpo);
		log_entry(ADDRESS_FROM_PAGETABLE, pa);
		return pa;
	}

	else
	{
		ppn = handle_page_fault(vpn);

		updatePT( vpn, ppn);

		updateTLB( tlbIndex, tlbTag, ppn);
		pa = constructPA( ppn, vpo);
		log_entry(PAGEFAULT, pa);
		return pa;
	}
}



/*******************************************************************************
*
*	Function that given a physical address and an array of chars, will request
*	data from the main memory and then will store the data (received in integers)
*	into an array of chars (dataBlock), to laters by inserted in a cache entry
*	as the cache memory is updated.
*
********************************************************************************/
void getDataBlock (int pa, unsigned char dataBlock[])
{
	int currentWordAdd = pa & ~0xf;	// getting address of first byte of memory page
	unsigned int currentWord;

	// Extracting the every by from whole memory page (byte by byte)

	// Getting each word (4 bytes) from the memory page
	for (int i=0; i<4; i++)
	{
		currentWord = get_word(currentWordAdd);	// getting one word
		
		// getting each byte from the currently visited word (in little endian order)
		for (int j=0; j<4; j++)
		{
			dataBlock[(i*4)+j] = (currentWord & 0xff);	// getting one byte
			currentWord = currentWord >> 8;
		}

		currentWordAdd += 4;			// calculating address of next word of data block	
	}

	return;
}



/*******************************************************************************
*
*	Function that updates the cache memory when there was a cache miss that forced
*	the memory system to get byte of data from the main memory.
*
********************************************************************************/
void updateCache( int cacheIndex, int cacheTag, unsigned char dataBlock[])
{
	//    checking if for the first invalid TLB entry
	int available = -1;
	int i = 0;
	
	while (available == -1 && i<2)	// looking for available invalid cache entry
	{
		if (cache[cacheIndex].cacheEntries[i].valid == 0)
			available = i;
		i++;
	}
	

	if (available == -1)		// if there was no invalid entry available
		available = cache[cacheIndex].leastRecent[0];// get least recently used entry

	//     updating the least recently accessed entry 
	cache[cacheIndex].cacheEntries[available].valid = 1;
	cache[cacheIndex].cacheEntries[available].tag = cacheTag;
	for( int i=0; i<PAGE_SIZE; i++)
		cache[cacheIndex].cacheEntries[available].data[i] = dataBlock[i];

	// updating the timestamps for the entries
	cache[cacheIndex].leastRecent[1] = available;
	cache[cacheIndex].leastRecent[0] = (available+1) % 2;

	return;
}



/*******************************************************************************
*
*	Function that given a cache index, cache tag and the offset, it fetches a 
*	byte of data from the cache.  If there is a cache miss, this function will
*	return 0.  Otherwise the function will return 1 and the byte that was fetched
*	will be stored in the variable by reference called byte.
*
********************************************************************************/
int getFromCache( char* byte, int cacheTag, int cacheIndex, int cacheOffset)
{
	int indexOfHit = 0;
	int cacheHit = FALSE;

	// parsing through the cache entries of the indexed cache set
	while ( !cacheHit  &&  indexOfHit<2 )
	{
		if ( (cache[cacheIndex].cacheEntries[indexOfHit].valid == 1) && 
				(cache[cacheIndex].cacheEntries[indexOfHit].tag == cacheTag) ) 	
		{	
			cacheHit = TRUE;	// it there is a cache hit we set TLB Hit flag to TRUE
			indexOfHit--;
		}

		indexOfHit++;
	}
	
	if (cacheHit)		// if there was a hit
	{	
		// retrieving the byte from the data block
		*byte = cache[cacheIndex].cacheEntries[indexOfHit].data[cacheOffset];

		// updating the timestamps for the entries
		cache[cacheIndex].leastRecent[1] = indexOfHit;
		cache[cacheIndex].leastRecent[0] = (indexOfHit+1) % 2;
	}

	return cacheHit;
}



/*******************************************************************************
*	Function that get a physical address and returns a byte which was retrieved
*	either from the cache or from the main memory (if there was a cache miss) 
*	which will require a cache update.
*
********************************************************************************/
char get_byte(int pa)
{
   char byte = 0; // Variable for the byte you will fetch.
	unsigned char dataBlock[PAGE_SIZE];
	int cacheTag, cacheIndex, cacheOffset;


	deconstructPA(pa, &cacheTag, &cacheIndex, &cacheOffset);//get components from VA

	if ( getFromCache( &byte, cacheTag, cacheIndex, cacheOffset))// if cache hit
	{	
		log_entry(DATA_FROM_CACHE, byte);
		return byte;
	}

	//  if there was a cache miss
	else
	{

		getDataBlock(pa, dataBlock);	// retrieves a whole data page (16 bytes) 
					//where the byte at the physical address is located in main memory.
		updateCache(cacheIndex, cacheTag, dataBlock);

		byte = dataBlock[cacheOffset];
		log_entry(DATA_FROM_MEMORY, byte); 
		return byte;
	}
}


//   THE END  //












