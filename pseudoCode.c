void L2COM(int set_index, int line_index, int type) {
    // Handles communication with L2 cache
    Open ComFile in append mode
    If ComFile is unavailable, print error and return
    If valid mode and type:
        Write appropriate L2 communication message based on type
    Close ComFile
}

void UpdateState_DataCache(int set, int way, int event) {
    // Updates the state of a cache line in the data cache
    Switch current state:
        Case 'M':
            Handle transitions for RESET or L2_EVICT change to Invalid state
            Handle transitions for L2_SNOOP_DATA change to Valid state
            else  Read or Write remain the same state of Modified line
        Case 'V':
            Handle transitions for RESET, WRITE, or L2_EVICT
            Reset --> Invalid state
            L1_WRITE_DATA --> Modified
            L2_EVICT --> Invalid
            else Valid
        Default:
            Handle transitions for READ or WRITE
            L1_READ_DATA --> Valid
            L1_WRITE_DATA --> Invalid
}

void UpdateState_InsCache(int set, int way, int event) {
    // Updates the state of a cache line in the instruction cache
    Switch current state:
        Case 'V':
            Handle transitions for RESET or L2_EVICT change state to Invalid
            L1_READ_INST --> Valid
        Default:
            Handle transitions for RESET or FETCH
            L1_READ_INST --> Valid
            RESET or L2_EVICT --> Invalid

}
void DataUpdateLRU(int set, int way) {
    // Updates LRU for data cache
    Adjust LRU values for all lines in the set
    Mark the current line as Most Recently Used (MRU)
}

void DataEvict(int set_index, int evict_tag) {
    // Evicts a line from the data cache
    For each line in the set:
        If line matches evict_tag:
            Handle eviction based on its current state
            Break
}

void DataHit(int set_index, int line_index, int event) {
    // Handles data cache hit
    Print type of hit (read or write)
    Update cache state, LRU, and hit statistics
}

void DataMiss(int set_index, int line_index, int new_tag, int event) {
    // Handles data cache miss
    Print type of miss
    Evict LRU line if required
    Fetch new data and update cache state, address, and LRU
    Update miss statistics
}

void DataReadWrite(int set_index, int req_tag, int event) {
    // Handles read/write operation for data cache
    Search for tag in the set:
        If found, call DataHit
        If not, call DataMiss with appropriate line
}

void DataClear() {
    // Clears all data cache entries
    For each set and line:
        Reset state and LRU values to default
}

void PrintDataCache() {
    // Prints all valid data cache entries
    For each set and line:
        If line is valid, print details
}

void InsEvict(int set_index, int evict_tag) {
    // Evicts a line from the instruction cache
    For each line in the set:
        If line matches evict_tag:
            Handle eviction and update state
}

void InsHit(int set_index, int line_index, int event) {
    // Handles instruction cache hit
    Print "Instruction cache: Read Hit"
    Update cache state and LRU
    Increment hit statistics
}

void InsMiss(int set_index, int line_index, int new_tag, int event) {
    // Handles instruction cache miss
    Print "Instruction cache: Read Miss"
    Update cache state, address, and LRU
    Increment miss statistics
}

void InsRead(int set_index, int req_tag, int event) {
    // Handles read operation for instruction cache
    Search for tag in the set:
        If found, call InsHit
        If not, call InsMiss with appropriate line
}

void InsClear() {
    // Clears all instruction cache entries
    For each set and line:
        Reset state and LRU values to default
}

void PrintInsCache() {
    // Prints all valid instruction cache entries
    For each set and line:
        If line is valid, print details
}

void PrintStats() {
    // Prints cache statistics for data and instruction caches
    Calculate and print hit/miss ratios and totals
}

void AddressSplit() {
    // Splits tmp_address into tag, index, and offset
    Extract tag, index, and offset for both data and instruction caches
}

void Menu(int key) {
    // Processes menu options
    AddressSplit()
    Switch key:
        Case READ_DATA:
            Increment read count and call DataReadWrite
        Case WRITE_DATA:
            Increment write count and call DataReadWrite
        Case READ_INST:
            Increment read count and call InsRead
        Case RESET:
            Clear caches and statistics
        Case PRINT:
            Print caches and statistics
        Default:
            Print "Invalid choice"
}

int main(int argc, char *argv[]) {
    // Main function for program initialization and loop
    Initialize mode and trace file
    Clear communication history and initialize caches
    Do:
        Open trace file and process each line
        Parse operation and address
        Call Menu with operation
    While mode is 2 and user provides new trace files
    Return 0
}
