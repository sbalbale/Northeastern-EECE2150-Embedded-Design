#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
using std::cout; using std::cin; using std::endl;

// Physical base address of FPGA Devices 
const unsigned int LW_BRIDGE_BASE 	= 0xFF200000;  // Base offset 

// Length of memory-mapped IO window 
const unsigned int LW_BRIDGE_SPAN 	= 0x00005000;  // Address map size

// Cyclone V FPGA device addresses
const unsigned int LEDR_BASE 		= 0x00000000;  // Leds offset 
const unsigned int SW_BASE 			= 0x00000040;  // Switches offset
const unsigned int KEY_BASE 		= 0x00000050;  // Push buttons offset

/** 
 * Write a 4-byte value at the specified general-purpose I/O location. 
 * 
 * @param pBase		Base address returned by 'mmap'. 
 * @parem offset	Offset where device is mapped. 
 * @param value	Value to be written. 
 */ 
void RegisterWrite(char *pBase, unsigned int reg_offset, int value) 
{ 
	* (volatile unsigned int *)(pBase + reg_offset) = value; 
	//volatile prevents the compiler from optimizing code
} 

/** 
 * Read a 4-byte value from the specified general-purpose I/O location. 
 * 
 * @param pBase		Base address returned by 'mmap'. 
 * @param offset	Offset where device is mapped. 
 * @return		Value read. 
 */ 
int RegisterRead(char *pBase, unsigned int reg_offset) 
{ 
	return * (volatile unsigned int *)(pBase + reg_offset); 
} 

/** 
 * Initialize general-purpose I/O 
 *  - Opens access to physical memory /dev/mem 
 *  - Maps memory into virtual address space 
 * 
 * @param fd	File descriptor passed by reference, where the result 
 *			of function 'open' will be stored. 
 * @return	Address to virtual memory which is mapped to physical, or MAP_FAILED on error. 
  */ 
char *Initialize(int *fd) 
{
	// Open /dev/mem to give access to physical addresses
	*fd = open( "/dev/mem", (O_RDWR | O_SYNC));
	if (*fd == -1)			//  check for errors in openning /dev/mem
	{
        cout << "ERROR: could not open /dev/mem..." << endl;
        exit(1);
	}
	
   // Get a mapping from physical addresses to virtual addresses
   char *virtual_base = (char *)mmap (NULL, LW_BRIDGE_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, *fd, LW_BRIDGE_BASE);
   if (virtual_base == MAP_FAILED)		// check for errors
   {
      cout << "ERROR: mmap() failed..." << endl;
      close (*fd);		// close memory before exiting
      exit(1);        // Returns 1 to the operating system;
   }
   return virtual_base;
}

/** 
 * Close general-purpose I/O. 
 * 
 * @param pBase	Virtual address where I/O was mapped. 
 * @param fd	File descriptor previously returned by 'open'. 
 */ 
void Finalize(char *pBase, int fd) 
{
	if (munmap (pBase, LW_BRIDGE_SPAN) != 0)
	{
      cout << "ERROR: munmap() failed..." << endl;
      exit(1);
	}
   close (fd); 	// close memory
}

int ReadAllSwitches(char *pBase) {
	return RegisterRead(pBase, SW_BASE) & 0x3FF;
}

int Read1Switch(char *pBase, int switchNum) {
	return (ReadAllSwitches(pBase) >> switchNum) & 1;
}

void WriteAllLeds(char *pBase, int value) {
	RegisterWrite(pBase, LEDR_BASE, value & 0x3ff);
}

void Write1Led(char *pBase, int ledNum, bool state) {
	int curLeds = RegisterRead(pBase, LEDR_BASE) & 0x3FF;
	int mask = -1 ^ (1 << ledNum); //1111011111
	curLeds = (curLeds & mask) | (state << ledNum);
	WriteAllLeds(pBase, curLeds);
}

// User Added Functions
// Secton 4 - Interfacing with Push Buttons
int PushButtonGet(char *pBase) {
	cout << RegisterRead(pBase, KEY_BASE) << endl;
	switch (RegisterRead(pBase, KEY_BASE)) {
		case 0:
			return -2;
		case 1:
			return 0;
		case 2:
			return 1;
		case 4:
			return 2;
		case 8:
			return 3;
		default:
			return -1;
	}
}



int main() 
{ 
	// Initialize 
	int fd; 
	char *pBase = Initialize(&fd);
	
	// User Added Functions
	// Secton 4 - Interfacing with Push Buttons
	int counter = ReadAllSwitches(pBase);
	WriteAllLeds(pBase, counter);

	int lastButtonState = -1;
	
	while (true) {
		cout << "lastButtonState: " << lastButtonState << " counter: " << counter << endl;
		int buttonState = PushButtonGet(pBase);
		if (buttonState != lastButtonState) {
			lastButtonState = buttonState;
			switch (buttonState) {
				case 0:
				// increment 1
					counter++;
					break;
				case 1:
				// decrement 1
					counter--;
					break;
				case 2:
				// shift right
					counter = counter >> 1;
					break;
				case 3:
				// shift left
					counter = counter << 1;
					break;
				case -1:
				// set to the value of the switches
					counter = ReadAllSwitches(pBase);
					break;
			}
		}
		WriteAllLeds(pBase, counter);
	}

	Finalize(pBase, fd);

}
