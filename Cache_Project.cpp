#include "L1cache.h"
// #include <windows.h>

FILE *fp;
const char *ComFile = "store_history.txt";
uint32_t data_tag, data_index, ins_tag, ins_index;
uint8_t data_offset, ins_offset;
char TraceFile[100]; // trace file name
char TraceLine[100]; // Store each line of trace file
int mode;			 // mode select and user option
char *token;		 // to split option and address
int n;				 // Option
unsigned int tmp_address;
struct Cache Ins_Cache[INS_SETS][INS_WAY], Data_Cache[DATA_SETS][DATA_WAY];
struct Stats DataStats, InsStats;
// Communication with L2
void L2COM(int set_index, int line_index, int type)
{

	// Write L2 communication to file "communication_history.txt"
	// In case dataWrite into dirty data, L1 will write to L2 and then read exclusive to write new data
	FILE *com = fopen(ComFile, "a");
	if (com == NULL)
	{
		printf("Could not open communication history file.\n");
		return;
	}
	if (mode != 0) {
    		if (type >= 0 && type <= 4) {
        		if (type == 2) // evict or snoop dirty line
           			fprintf(com, "\n\t    Write to L2                      <0x%X>", Data_Cache[set_index][line_index].address); // flush
        		else if (type == 0) // READ OPERATION
            			fprintf(com, "\n\t    Read from L2                     <0x%X>", tmp_address);
        		else if (type == 1) // WRITE OPERATION
            			fprintf(com, "\n\t    Read for Ownership from L2       <0x%X>", tmp_address);
        		else if (type == 3) // instruction communicate
            			fprintf(com, "\n\t    Read from L2                     <0x%X>", tmp_address);
    	} else if (type == -1) {
        	fprintf(com, "\n\t    Do nothing");
    	}
	}
	if (com != NULL) {
    		fclose(com);
	}
}

void PrintL2COM()
{
	FILE *readHistory = fopen(ComFile, "a");
	char message[100];
	if (mode != 0)
	{
		fprintf(readHistory, "\n\t\t\t  L2 COMMUNICATION: ");
		printf("\n\t\t\t  L2 COMMUNICATION: ");
		// while(fgets(message, sizeof(message), readHistory)) {
		// 	printf("%s", message);
		// }
	}
	fclose(readHistory);
}
//MVI protocol:
/*
Event in this cache:
    - Reset: reset the cache line(system or cache reset)
    - L1_Read_Data: Read data from L1 cache
    - L1_Write_Data: Write data to L1 cache
    - L2_Evict: Eviction from L2 cache
    - L2_SNOOP_DATA: 
*/
//Function for data cache
void UpdateState_DataCache(int set, int way, int n){
    //Specify cache and set way in 2D array 
	switch(Data_Cache[set][way].state) {
		// in case current state is Modified
		case 'M': 
            //Use n to represents the events or operations that triggers the state change
			if(n == RESET || n == L2_EVICT) { // (6) If reset the data in this cache line and the L2 is evict data to 
				// Reset
				Data_Cache[set][way].state = 'I'; //Change the state to from modified to invalid
			}
            //If the cache line is in Modified and snooped by L2, the data written back to L2 ensure inclusivity(perform write back operation)
			else if(n == L2_SNOOP_DATA) { // (8)
				// Read from L2 (snoop data)
				Data_Cache[set][way].state = 'V'; //Change to "V" state -> Hold a clean copy that data matches L2 have
                //The cache still have a data but not exclusive owernship
			}
            //The events does not affect to read write at Modified state
			else { // Data Read or Data Write dont change state (5)
				Data_Cache[set][way].state = 'M'; //Remain at the same state of Modified line
			}
			break;
		
		// in case current state is Valid
		case 'V': 
			if(n == RESET) { // (2)
				// Reset
				Data_Cache[set][way].state = 'I';
			}
			else if(n == L1_WRITE_DATA) { // (3)
				// Data Write (Write hit)
				Data_Cache[set][way].state = 'M';
			}
            else if(n == L2_EVICT){
                Data_Cache[set][way].state = 'I';
            }
			else { // Data Read dont change state (4)
				Data_Cache[set][way].state = 'V';
			}
			break;
		
		// in case current state is Invalid
		default:
			if(n == L1_READ_DATA || n == L1_WRITE_DATA) { // (1)
				// Data Read or first Write
				Data_Cache[set][way].state = 'V';
			}
			else // Evict from L2 or Reset
				Data_Cache[set][way].state = 'I';
			break;	
	}
}

//Function for instruction cache 
void UpdateState_InsCache(int set, int way, int n){
	switch(Ins_Cache[set][way].state) {	
		// in case current state is Valid
		case 'V': 
			if(n == RESET || n == L2_EVICT) { // (2)
				// Reset
				Ins_Cache[set][way].state = 'I';
			}
			else if(n == L1_READ_INST) { // (3)
				// Instruction fetch
				Ins_Cache[set][way].state = 'V';
			}
			break;
			
		// in case current state is Invalid
		default:
		//Line transitions to valid because the data is fetched and made usable
			if(n == L1_READ_INST) { // (1)
				// Instruction fetch
				Ins_Cache[set][way].state = 'V';
			}
			else if(n == RESET || n == L2_EVICT) { // (4)
				// Reset
				Ins_Cache[set][way].state = 'I';
			}
			break;
	}
}

//LRU policy
//This function use for update LRU values for the instruction cache, when a line is accessed or written 
void InsUpdateLRU(int set, int way) //The set and way specification the location of the cache. The set is bit with a way is 2 way for instruction cache(0,1)
{	
	if(Ins_Cache[set][way].lru == -1) { // first time will be Write Through, this is first time access for (lru == -1)
		// From initialized cache, so -1 means empty way
		Ins_Cache[set][way].lru = INS_WAY - 1; // Now this line is MRU, lines get the highest rank (INS_WAY-1) while other lines decrement their ranks
        //INS_WAY -1 assign MRU rank to the accessed cache line

		for(int i = way-1; i >= 0; i--) { //Decrement the rank of the preceding lines in the same set(way -1 to 0) 
			// Decrementing LRU value of each line before 
			Ins_Cache[set][i].lru--;		
		}
	}
    //Subsequent access (Hit or miss) LRU >= 0
    //Loop through all cache lines in the set
	else if(Ins_Cache[set][way].lru >= 0) { // a hit or miss
		for(int i = 0; i < INS_WAY; i++) { // Search all lines
			// every line has LRU value greater than current LRU will be decremented
			if(Ins_Cache[set][i].lru > Ins_Cache[set][way].lru)
				Ins_Cache[set][i].lru--; 
		}
        //update the accessed line LRU to INS_WAY -1 (making it the MRU)
		Ins_Cache[set][way].lru = INS_WAY - 1; // Now this line is MRU
	}	
}
//The same with InsUpdateLRU
void DataUpdateLRU(int set, int way)
{
	if(Data_Cache[set][way].lru == -1) { // first time will be Write Through
		// From initialized cache, so -1 means empty way
		Data_Cache[set][way].lru = DATA_WAY - 1; // Now this line is MRU
		for(int i = way-1; i >= 0; i--) { 
			// Decrementing LRU value of each line before 
			Data_Cache[set][i].lru--;		
		}
	}
	else if(Data_Cache[set][way].lru >= 0) { // a hit or miss
		for(int i = 0; i < DATA_WAY; i++) { // Search all lines
			// every line has LRU value greater than current LRU will be decremented
			if(Data_Cache[set][i].lru > Data_Cache[set][way].lru)
				Data_Cache[set][i].lru--; 
		}
		Data_Cache[set][way].lru = DATA_WAY - 1; // Now this line is MRU
	}	
}
//Data Cache:
/*
- L1 data cache
- 4-way -> LRU is 2 bit, 0-3


// State
#define	M				0
#define	V				1
#define	I				2
*/

//Function to deal with evict data from a data cache
void DataEvict(int set_index, uint32_t evict_tag) { //Identify for a specific line in an specific set to evict, set_index cache set identify
	//line_index represents for the first way in the cache set. Still ++ until reach 4 way, after each iteration. line_index incremented by 1 to move to the nex way
	for(int line_index = 0; line_index < DATA_WAY; line_index++) { //Loop through in 1 set and check for all lines in the cache set (DATA_WAY)
		// Data_Cache[set_index][line_index].lru = -1; // default LRU value when empty
		if(Data_Cache[set_index][line_index].tag == evict_tag) { //If statements define for the cache line's tag matches evict_tag
			//Data_Cache[set_index][line_index].address >>= 32;
			//handle dirty bit	
			if(Data_Cache[set_index][line_index].state == 'M') {
				printf("Evict dirty data of way %d\n", line_index);
				L2COM(set_index, line_index, 2); //write back to L2 to maintain inclusivity
				UpdateState_DataCache(set_index, line_index, L2_EVICT);
			}
			else if(Data_Cache[set_index][line_index].state == 'V'){
				printf("Evict way %d\n", line_index);
                UpdateState_DataCache(set_index, line_index, L2_EVICT);
                
			}
			break; //Exit loop
		}
	}
}

// If the tag which we are finding, present in the set -> updating the state and LRU
// Void use for data cache hits occurs. THe requested data is found in the specified cache line within set and line 
void DataHit(int set_index, int line_index, int n) {
    // Print hit type based on the operation
    if (n == 0) {
        printf("Data cache: Read Hit\n"); // Read operation
    } else {
        printf("Data cache: Write Hit\n"); // Write operation
    }
    // Update the state of the cache line 
	/*Data hit occurs when the requested data found in the cache. Read hits keep the data in Valid
	  Write hit often transition the cache line to a Modified state => has been written into L1 data cache and differ from L2
	  Tracking write operation: Write hit occurs , the cache need to ensure that "data in this line has been modified" => important that this line need to write back to L2 cache => ensure consistency*/
    UpdateState_DataCache(set_index, line_index, n); //Update the state of cache line
    // Do not change address unless byte_offset is different
    if (Data_Cache[set_index][line_index].byte_offset != data_offset) {
        Data_Cache[set_index][line_index].address = tmp_address;
        Data_Cache[set_index][line_index].byte_offset = data_offset;
    }
    // Update this cache line's LRU to MRU
    DataUpdateLRU(set_index, line_index);
    // Notify L2 about the access
    L2COM(set_index, line_index, -1); //-1 is the types do not doing anything to L2 communication
    // Increment cache hit statistics
    DataStats.Cache_Hit++;
}


//Void to handle Data Miss operation => Necessary actions to fetch the data, evict 
void DataMiss(int set_index, int line_index, int new_tag, int n, char *debugMess) {
	//Handling a read or write miss 
	if(n == 0) { //n==0 to indicate a read miss
		printf("Data cache: Read Miss %s", debugMess); // debug messages printed for easier or logging
	}
	//If a write operation. On the first start it will write through to the cache
	else if(n == 1) {
		printf("Data cache: Write Miss but Write Through because first write %s", debugMess); // debug messages
	}
	/*Identify the cache line to evict. The tag of line currently occupying the selected cache location with set_index and line_index */
	/*The Data Request is not in CPU => not present in the cache => Must be fetched from a lower memory level => free up space for set when full*/
	int evict_tag = Data_Cache[set_index][line_index].tag; // find old tag to evict	=> allow me to ensure what data is being evicted, if data is if dirty(modified line)must be written back to L2 
	//Dirty data(modified line) in L1 is written back to L2 during eviction
	L2COM(set_index, line_index, n); // read(read operation) or read for owenership(write operation from L2 to L1)
	//Indicate if lru != -1 (this line is valid and not empty) -> eviction is required
	if(Data_Cache[set_index][line_index].lru != -1) { // skip when set contain empty way
		DataEvict(set_index, evict_tag); //So we will evict this line 	
	}
	//Update the cache line with the new data
	Data_Cache[set_index][line_index].address = tmp_address; //Full address of new data
	Data_Cache[set_index][line_index].tag = new_tag; //The new tag value extracted from 
	Data_Cache[set_index][line_index].set = set_index;
	Data_Cache[set_index][line_index].byte_offset = data_offset;
	//Write operation L1 when write through must be written into L2 maintain inclusivity
	if(n == 1){ 
		L2COM(set_index, line_index, 4); // write through in case first write
	}
	/*Got DataMiss, after fetching data from L2 */
	//Update the state of cache line making valid and modified
	/*On a L1_WRITE_DATA V become M , I become V 
	On a L1_READ_DATA I fetched data from L2, that means it will update to V line
	On a Reset or Eviction => If does not have enough space => May be will evict data in M V => It will become invalid and make space for new data income*/

	UpdateState_DataCache(set_index, line_index, n);
	//Update it become MRU, after evict data 
	DataUpdateLRU(set_index, line_index); // update this line to MRU
	//Count for the cache miss
	DataStats.Cache_Miss++;
}

//Function to handle  data/write operation => Results in cache read/write operation
void DataReadWrite(int set_index, uint32_t req_tag, int n) {
	//keep track of empty line (a cache line in the currrent set with state "I") => Used to store new data in cache of cache miss
	int empty_line = -1;
	//Search for suitable cache ways for a suitable line
	//The loop to iterates over all DATA_WAY 
	for(int i = DATA_WAY-1; i >= 0; i--) { // find suitable line_index
		if(Data_Cache[set_index][i].state == 'I')  //If find a line is invalid => Potential target for storing new data during a cache miss
			empty_line = i; // least line_index empty line when no hit
		else { // check valid
		//If the current lines matches the req_tag, it means the requested data is already in cache => cache hit 
			if(Data_Cache[set_index][i].tag == req_tag) { // a hit
				// Dont need to change address since a hit
				DataHit(set_index, i, n);
				return; // out function when hit -> saving runtime
			}
		}
	}	
	// If this function is still running -> definitely get a miss
	if(empty_line >= 0) {
		// Contain an empty line to fill data
		DataMiss(set_index, empty_line, req_tag, n, "(unoccupied)\n");
	}
	else {
		// Line was fulled -> evict LRU line
		for(int line_index = 0; line_index < DATA_WAY; line_index++) { // find suitable line_index
			if(Data_Cache[set_index][line_index].lru == 0) { // if this is LRU line_index
				DataMiss(set_index, line_index, req_tag, n, "(conflict)\n"); // read/write and update address
				return; // out function for saving runtime
			}
		}
	}
}
	
// Clear the entire data cache -> to make it default state -> ensure cache starts in a clean, invalid state
void DataClear(void) {
	//Loop through over 16384 set to 
	for(int set_index = 0; set_index < DATA_SETS; set_index++) {
		//Loop through over all cache lines(way) in the current set => Corresspond to a single entry in the cache
		for(int line_index = 0; line_index < DATA_WAY; line_index++) {
			Data_Cache[set_index][line_index].lru = -1; // default LRU value when empty
			Data_Cache[set_index][line_index].state = 'I'; // default state
		}
	}
}

// Print the content of avaiable data in line
void PrintDataCache(void)
{
	FILE *com = fopen(ComFile, "a"); // Open ComFile in append mode
	if (com == NULL)
	{
		printf("Error: Could not open communication file for writing.\n");
		return;
	}
	fprintf(com, "\n\n===========================================");
	fprintf(com, "\n\t\t\t\t\t\t\t\t\t\t\t\t DATA CACHE");
	fprintf(com, "\n===========================================\n");

	printf("\n\n======================================================================");
	printf("\n\t\t\t     DATA CACHE");
	printf("\n======================================================================\n");
	for (int set_index = 0; set_index < DATA_SETS; set_index++)
	{
		for (int line_index = 0; line_index < DATA_WAY; line_index++)
		{
			if (Data_Cache[set_index][line_index].lru != -1)
			{
				char currState = Data_Cache[set_index][line_index].state;
				if (line_index == 0)
				{
					fprintf(com, "------------------------------SET %d--------------------------------\n", set_index);
					printf("------------------------------SET %d--------------------------------\n", set_index);
				}
				fprintf(com, "WAY: %d | VALID: %d | DIRTY: %d | TAG: %04X | LRU: %d | ADDRESS: 0x%X \n",
						line_index,
						!(currState == 'I'),
						currState == 'M',
						Data_Cache[set_index][line_index].tag,
						Data_Cache[set_index][line_index].lru,
						Data_Cache[set_index][line_index].address);
				printf("WAY: %d | VALID: %d | DIRTY: %d | TAG: %04X | LRU: %d | ADDRESS: 0x%X \n",
					   line_index,
					   !(currState == 'I'),
					   currState == 'M',
					   Data_Cache[set_index][line_index].tag,
					   Data_Cache[set_index][line_index].lru,
					   Data_Cache[set_index][line_index].address);
			}
		}
	}
	fclose(com);
}
// Instruction Cache:
// Function to evict cache set => Look for the line to evict
void InsEvict(int set_index, int evict_tag)
{ // Identify data within a set
	// Loop through all lines in the set,
	for (int line_index = 0; line_index < INS_WAY; line_index++)
	{ // line_index iterates over all line, INS_WAY represent the way for the set(the line in 1 set)
		// Data_Cache[set_index][line_index].lru = -1; // default LRU value when empty
		if (Ins_Cache[set_index][line_index].tag == evict_tag)
		{ // line_index => identified by set_index. If match evict_tag => match found => proceed to handle eviction
			// Ins_Cache[set_index][line_index].address >>= 32;
			printf("Evict instruction in way %d\n", line_index);   // print line_index where the eviction occurs => track for cache line
			UpdateState_InsCache(set_index, line_index, L2_EVICT); // Make the line from valid to invalid
		}
	}
}

// If the tag which we are finding, present in the set -> updating the state and LRU.
// set_index: identifier the cache set and specific line_index
void InsHit(int set_index, int line_index, int n)
{ // represents the types of operation triggered the cache(instruction fetch)
	printf("Instruction cache: Read Hit\n");
	// Do not change address
	UpdateState_InsCache(set_index, line_index, n);
	InsUpdateLRU(set_index, line_index); // update this line to MRU
	L2COM(set_index, line_index, -1);
	InsStats.Cache_Hit++;
}

// When we get a miss
//CPU attempts to fetch an instruction from L1 instruction cache and not in cache => Send a request to L2 cache to receive the instruction
// If in L2 -> supplies for L1 instruction, if not request from lower memory 
void InsMiss(int set_index, int line_index, int new_tag, int n, char *debugMess)
{
	//Read operation of instruction cache, when read miss occurs. Means that data is not in the cache
	printf("Instruction cache: Read Miss %s", debugMess); // debug messages
	//Write to the line invalid -> update to valid state
	UpdateState_InsCache(set_index, line_index, n);
	//Store the address
	Ins_Cache[set_index][line_index].address = tmp_address;
	// Change new address for line, with new_tag and for specific set -> store the byte_offset bits 
	Ins_Cache[set_index][line_index].tag = new_tag;
	Ins_Cache[set_index][line_index].set = set_index;
	Ins_Cache[set_index][line_index].byte_offset = ins_offset;
	//Update the line to MRU with value is 1
	InsUpdateLRU(set_index, line_index); // update this line to MRU
	//n == 3 Instruction Communicate with L2
	L2COM(set_index, line_index, 3);
	InsStats.Cache_Miss++;
}

// Handles the flow for the instruction cache
void InsRead(int set_index, int req_tag, int n)
{
	int empty_line = -1;
	for (int i = INS_WAY - 1; i >= 0; i--)
	{ // find suitable line_index
		if (Ins_Cache[set_index][i].state == 'I')
			empty_line = i; // store least line_index empty line when no hit
		else
		{ // check valid
			if (Ins_Cache[set_index][i].tag == req_tag)
			{ // a hit
				// Dont need to change address since a hit
				InsHit(set_index, i, n);
				return; // out function when hit -> saving runtime
			}
		}
	}

	// If this function is still running -> definitely get a miss
	if (empty_line >= 0)
	{
		// Contain an empty line to fill instruction
		InsMiss(set_index, empty_line, req_tag, n, "(unoccupied)\n");
	}
	else
	{
		// Line was fulled -> evict LRU line
		for (int i = 0; i < INS_WAY; i++)
		{ // find suitable line_index
			if (Ins_Cache[set_index][i].lru == 0)
			{ // if this is LRU line_index
				InsMiss(set_index, i, req_tag, n, "(conflict)\n");
				return; // out function for saving runtime
			}
		}
	}
}

// Clear the entire instruction cache
void InsClear(void)
{
	for (int set_index = 0; set_index < INS_SETS; set_index++)
	{
		for (int line_index = 0; line_index < INS_WAY; line_index++)
		{
			Ins_Cache[set_index][line_index].lru = -1;	  // default LRU value when empty
			Ins_Cache[set_index][line_index].state = 'I'; // default state
		}
	}
}

// Print the content of avaiable instruction in line
void PrintInsCache(void)
{
	FILE *com = fopen(ComFile, "a"); // Open ComFile in append mode
	if (com == NULL)
	{
		printf("Error: Could not open communication file for writing.\n");
		return;
	}
	fprintf(com, "\n\n================================================");
	fprintf(com, "\n\t\t\t\t\t\t\t\t\t INSTRUCTION CACHE");
	fprintf(com, "\n================================================\n");

	printf("\n\n======================================================================");
	printf("\n\t\t\t  INSTRUCTION CACHE");
	printf("\n======================================================================\n");
	for (int set_index = 0; set_index < INS_SETS; set_index++)
	{
		for (int line_index = 0; line_index < INS_WAY; line_index++)
		{
			if (Ins_Cache[set_index][line_index].state != 'I')
			{
				if (line_index == 0)
				{
					fprintf(com, "------------------------------SET %d--------------------------------\n", set_index);
					printf("------------------------------SET %d--------------------------------\n", set_index);
				}
				fprintf(com, "      WAY: %d | VALID: %d | DIRTY: 0 | TAG: %X LRU: %d | ADDRESS: 0x%X \n",
						line_index,
						(Ins_Cache[set_index][line_index].state == 'V'),
						Ins_Cache[set_index][line_index].tag,
						Ins_Cache[set_index][line_index].lru,
						Ins_Cache[set_index][line_index].address);
				printf("      WAY: %d | VALID: %d | DIRTY: 0 | TAG: %X LRU: %d | ADDRESS: 0x%X \n",
					   line_index,
					   (Ins_Cache[set_index][line_index].state == 'V'),
					   Ins_Cache[set_index][line_index].tag,
					   Ins_Cache[set_index][line_index].lru,
					   Ins_Cache[set_index][line_index].address);
			}
		}
	}
	fclose(com);
}
void StatsClear()
{
	DataStats.Cache_Read = 0;
	DataStats.Cache_Write = 0;
	DataStats.Cache_Hit = 0;
	DataStats.Cache_Miss = 0;

	InsStats.Cache_Read = 0;
	InsStats.Cache_Write = 0;
	InsStats.Cache_Hit = 0;
	InsStats.Cache_Miss = 0;
	return;
}
void PrintStats()
{
    FILE *com = fopen(ComFile, "a"); 
    if (com == NULL)
    {
        printf("Error: Could not open communication file for writing.\n");
        return;
    }

    float ratio;
    float total;

    total = InsStats.Cache_Hit + InsStats.Cache_Miss;
    if (total == 0)
    {
        ratio = 0; 
    }
    else
    {
        ratio = InsStats.Cache_Hit / total * 100;
    }

    printf("\n~~~~~~~~~~~~~ Instruction Statistics ~~~~~~~~~~~~~\n");
    printf("Number of cache reads: %d\n", InsStats.Cache_Read);
    printf("Number of cache writes: %d\n", InsStats.Cache_Write);
    printf("Number of cache hits: %d\n", InsStats.Cache_Hit);
    printf("Number of cache misses: %d\n", InsStats.Cache_Miss);
    printf("Hit ratio percentage: %.2f %% \n", ratio);

    fprintf(com, "\n~~~~~~~~~~~~~ Instruction Statistics ~~~~~~~~~~~~~\n");
    fprintf(com, "Number of cache reads: %d\n", InsStats.Cache_Read);
    fprintf(com, "Number of cache writes: %d\n", InsStats.Cache_Write);
    fprintf(com, "Number of cache hits: %d\n", InsStats.Cache_Hit);
    fprintf(com, "Number of cache misses: %d\n", InsStats.Cache_Miss);
    fprintf(com, "Hit ratio percentage: %.2f %% \n", ratio);

    fflush(com); 

  
    total = DataStats.Cache_Hit + DataStats.Cache_Miss;
    if (total == 0)
    {
        ratio = 0; 
    }
    else
    {
        ratio = DataStats.Cache_Hit / total * 100;
    }

    printf("\n~~~~~~~~~~~~~ Data Statistics ~~~~~~~~~~~~~\n");
    printf("Number of cache reads: %d\n", DataStats.Cache_Read);
    printf("Number of cache writes: %d\n", DataStats.Cache_Write);
    printf("Number of cache hits: %d\n", DataStats.Cache_Hit);
    printf("Number of cache misses: %d\n", DataStats.Cache_Miss);
    printf("Hit ratio percentage: %.2f %% \n", ratio);

    fprintf(com, "\n~~~~~~~~~~~~~ Data Statistics ~~~~~~~~~~~~~\n");
    fprintf(com, "Number of cache reads: %d\n", DataStats.Cache_Read);
    fprintf(com, "Number of cache writes: %d\n", DataStats.Cache_Write);
    fprintf(com, "Number of cache hits: %d\n", DataStats.Cache_Hit);
    fprintf(com, "Number of cache misses: %d\n", DataStats.Cache_Miss);
    fprintf(com, "Hit ratio percentage: %.2f %% \n", ratio);

    fflush(com); 

    fclose(com); 
}

void AddressSplit()
{
	ins_tag = tmp_address >> 20;			// Extract top 12 bits (bits 20-31)
	ins_index = (tmp_address << 12) >> 18;	// Extract middle 14 bits (bits 6-19)
	ins_offset = (tmp_address << 26) >> 26; // Extract lowest 6 bits (bits 0-5)

	data_tag = tmp_address >> 20;			 // Extract top 12 bits (bits 20-31)
	data_index = (tmp_address << 12) >> 18;	 // Extract middle 14 bits (bits 6-19)
	data_offset = (tmp_address << 26) >> 26; // Extract lowest 6 bits (bits 0-5)
}

void menu(int key)
{
	//	if(mode == 2 && key < 8) {
	//		printf("\n\t----> Enter cache address: ");
	//		//fgets(TraceLine, sizeof(TraceLine), stdin);
	//		scanf("%s", TraceLine);
	//		tmp_address = strtoul(TraceLine, NULL, 16);
	//	}

	AddressSplit();
	switch (key)
	{
	case L1_READ_DATA:
		DataStats.Cache_Read++;
		DataReadWrite(data_index, data_tag, key);
		break;

	case L1_WRITE_DATA:
		DataStats.Cache_Write++;
		DataReadWrite(data_index, data_tag, key);
		break;

	case L1_READ_INST:
		InsStats.Cache_Read++;
		InsRead(ins_index, ins_tag, key);
		break;

	case L2_EVICT:
		// updateState_insCache(ins_index, ins_tag, n);
		DataEvict(data_index, data_tag);
		InsEvict(ins_index, ins_tag);
		break;

	case RESET:
		InsClear();
		DataClear();
		StatsClear();
		break;

	case PRINT:
		PrintDataCache();
		PrintInsCache();
		PrintL2COM();
		PrintStats();
		system("pause");
		break;

		// case EXIT:
		//	printf("\nExit program...");
		//	break;

	default:
		printf("\nYour choice is unavaiable!!");
		break;
	}


	/*Uncomment the following lines in order to  print step by step */ 

	// PrintDataCache();
	// PrintInsCache();
	// PrintL2COM();
	// PrintStats();
	// system("pause");
	// system("cls");
}

int main(int argc, char *argv[])
{
	if (argc == 3)
	{								// Expecting mode and trace file as command-line arguments
		mode = atoi(argv[1]);		// Convert mode to integer
		strcpy(TraceFile, argv[2]); // Copy the trace file name
	}
	else
	{ // Prompt user for mode if not provided
		printf("\t=============================================================================\n");
		printf("\tMode 0: Displays statistics summary and responses to commands from trace file\n");
		printf("\tMode 1: Display mode 0 and the L2 communication from trace file\n");
		printf("\tMode 2: Demo multiple trace file with mode 1\n");
		printf("\t=============================================================================\n");
		printf("\t-----> Your select: ");
		scanf("%d", &mode);
		if (mode < 0 || mode > 2)
		{
			printf("\nInvalid mode selected! Exiting program.\n");
			return -1;
		}
		printf("\nEnter the trace file name: ");
		scanf("%99s", TraceFile);
	}

	// Clear previous communication history
	fclose(fopen(ComFile, "w"));
	InsClear();
	DataClear();

	// Main processing loop
	do
	{
		fp = fopen(TraceFile, "r");
		if (!fp)
		{
			printf("\nFile '%s' does not exist! Please try again.\n", TraceFile);
			return -1; // Exit if the file does not exist
		}

		while (fgets(TraceLine, sizeof(TraceLine), fp))
		{
			printf("\n%s", TraceLine);
			token = strtok(TraceLine, " ");
			n = atoi(token);
			while (token != NULL)
			{
				tmp_address = strtoul(token, NULL, 16);
				token = strtok(NULL, " ");
			}
			if (tmp_address)
			{
				menu(n);
			}
			else if (n == 9 && tmp_address == 0)
			{
				menu(9);
			}else if (n == 8 && tmp_address == 0)
			{
				menu(8);
			}
		}
		fclose(fp);

		// For mode 2, allow the user to enter another trace file
		if (mode == 2)
		{
			printf("\nEnter another trace file name (or type 'exit' to quit): ");
			scanf("%99s", TraceFile);
			if (strcmp(TraceFile, "exit") == 0)
			{
				break; // Exit the loop if user types 'exit'
			}
		}
	} while (mode == 2);

	return 0;
}
