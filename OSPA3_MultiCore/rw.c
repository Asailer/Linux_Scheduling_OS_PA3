/*
 * File: rw.c
 * Author: Conrad Hougen adapted from rw.c by Andy Sayler
 * Project: CSCI 3753 Programming Assignment 3
 * Description: A small i/o bound program to copy N bytes from an input
 *              file to an output file. May read the input file multiple
 *              times if N is larger than the size of the input file.
 * 				Forks multiple child processes and changes the
 * 				scheduling policy based on command-line arguments.
 */

/* Include Flags */
#define _GNU_SOURCE

/* System Includes */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sched.h>

/* Local Defines */
#define MAXFILENAMELENGTH 80
#define DEFAULT_INPUTFILENAMEBASE "input/rwinput"
#define DEFAULT_OUTPUTFILENAMEBASE "output/dat"
#define MAX_CHILDREN 200
char STATOUT_FILE[MAXFILENAMELENGTH];


int main(int argc, char* argv[])
{
	FILE *statout_fp = NULL;
	int num_children;
    pid_t pid;
    int status;
    int policy;
    int i;
    int rv;
    int inputFD;
    int outputFD;
    char inputFilename[MAXFILENAMELENGTH];
    char inputFilenameBase[MAXFILENAMELENGTH];
    char outputFilename[MAXFILENAMELENGTH];
    char outputFilenameBase[MAXFILENAMELENGTH];
    struct sched_param param;
    struct rusage r_usage; // struct to collect statistics

    ssize_t transfersize = 0;
    ssize_t blocksize = 0; 
    char* transferBuffer = NULL;
    ssize_t buffersize;

    ssize_t bytesRead = 0;
    ssize_t totalBytesRead = 0;
    int totalReads = 0;
    ssize_t bytesWritten = 0;
    ssize_t totalBytesWritten = 0;
    int totalWrites = 0;
    int inputFileResets = 0;
    
    // Process program arguments to select run-time parameters
    if(argc != 6)
    {
		printf("%d args received...expected 6\n", argc);
		exit(EXIT_FAILURE);
    }
    transfersize = atol(argv[1]);
	if(transfersize < 1)
	{
	    printf("Bad transfersize value\n");
	    exit(EXIT_FAILURE);
	}
    
    // Set supplied block size
	blocksize = atol(argv[2]);
	if(blocksize < 1)
	{
	    printf("Bad blocksize value\n");
	    exit(EXIT_FAILURE);
    }
    
    // Confirm blocksize is multiple of and less than transfersize
    if(blocksize > transfersize)
    {
		printf("blocksize can not exceed transfersize\n");
		exit(EXIT_FAILURE);
    }
    if(transfersize % blocksize)
    {
		printf("blocksize must be multiple of transfersize\n");
		exit(EXIT_FAILURE);
    }
    
    // Get scheduling policy from command line
	if(!strcmp(argv[3], "SCHED_OTHER")){
	    policy = SCHED_OTHER;
	}
	else if(!strcmp(argv[3], "SCHED_FIFO")){
	    policy = SCHED_FIFO;
	}
	else if(!strcmp(argv[3], "SCHED_RR")){
	    policy = SCHED_RR;
	}
	else{
	    printf("Unhandled scheduling policy\n");
	    exit(EXIT_FAILURE);
	}
	
	// Get number of child processes from command line
	num_children = atol(argv[4]);
	if(num_children < 1 || num_children > MAX_CHILDREN)
	{
		printf("Incorrect number of children to fork: %d\n", num_children);
		exit(EXIT_FAILURE);
	}
	
	// Get the output file name corresponding to trial number
	i = atol(argv[5]);
	sprintf(STATOUT_FILE, "test_results/test%d.txt", i);
    
    // 
	if(strnlen(DEFAULT_INPUTFILENAMEBASE, MAXFILENAMELENGTH) >= MAXFILENAMELENGTH){
	    fprintf(stderr, "Default input filename base too long\n");
	    exit(EXIT_FAILURE);
	}
	strncpy(inputFilenameBase, DEFAULT_INPUTFILENAMEBASE, MAXFILENAMELENGTH);

    //
	if(strnlen(DEFAULT_OUTPUTFILENAMEBASE, MAXFILENAMELENGTH) >= MAXFILENAMELENGTH){
	    fprintf(stderr, "Default output filename base too long\n");
	    exit(EXIT_FAILURE);
	}
	strncpy(outputFilenameBase, DEFAULT_OUTPUTFILENAMEBASE, MAXFILENAMELENGTH);

	// Set process to max priority for given scheduler
    param.sched_priority = sched_get_priority_max(policy);
    
    // Set new scheduler policy
    fprintf(stdout, "Current Scheduling Policy: %d\n", sched_getscheduler(0));
    fprintf(stdout, "Setting Scheduling Policy to: %d\n", policy);
    if(sched_setscheduler(0, policy, &param)){
		perror("Error setting scheduler policy");
		exit(EXIT_FAILURE);
    }
    fprintf(stdout, "New Scheduling Policy: %d\n", sched_getscheduler(0));
    

	// Fork num_children io-bound processes
	for(i = 0; i < num_children; ++i)
	{
		if((pid = fork()) == -1)
		{
			printf("Fork failed with i = %d\n", i);
			exit(EXIT_FAILURE);
		}
		else if(pid == 0)
		{
			// Allocate buffer space
			buffersize = blocksize;
			if(!(transferBuffer = malloc(buffersize*sizeof(*transferBuffer))))
			{
				printf("Failed to allocate transfer buffer\n");
				exit(EXIT_FAILURE);
			}
	
			// Open Input File Descriptor in Read Only mode
			// Files 1 to 200 must be created for this to work
			// Names are rwinput-1 to rwinput-200 in input folder
			// i should be unique in each child process depending on when
			// the child was forked
			rv = snprintf(inputFilename, MAXFILENAMELENGTH, "%s-%d",
					inputFilenameBase, i+1);
			if(rv > MAXFILENAMELENGTH)
			{
				printf("Input filename length exceeds limit of %d characters.\n",
					MAXFILENAMELENGTH);
				exit(EXIT_FAILURE);
			}
			else if(rv < 0){
				printf("Failed to generate input filename\n");
				exit(EXIT_FAILURE);
			}
			if((inputFD = open(inputFilename, O_RDONLY | O_SYNC)) < 0)
			{
				printf("Failed to open input file\n");
				exit(EXIT_FAILURE);
			}
			

			// Open Output File Descriptor in Write Only mode with standard permissions
			rv = snprintf(outputFilename, MAXFILENAMELENGTH, "%s-%d",
				outputFilenameBase, i+1);    
			if(rv > MAXFILENAMELENGTH)
			{
				printf("Output filename length exceeds limit of %d characters.\n",
					MAXFILENAMELENGTH);
				exit(EXIT_FAILURE);
			}
			else if(rv < 0){
				printf("Failed to generate output filename\n");
				exit(EXIT_FAILURE);
			}
			if((outputFD = open(outputFilename,
				O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0)
			{
				printf("Failed to open output file\n");
				exit(EXIT_FAILURE);
			}

			// Print Status
			printf("Reading from %s and writing to %s\n", inputFilename, outputFilename);

			// Read from input file and write to output file
			do{
				// Read transfersize bytes from input file
				bytesRead = read(inputFD, transferBuffer, buffersize);
				if(bytesRead < 0){
					printf("Error reading input file\n");
					exit(EXIT_FAILURE);
				}
				else{
					totalBytesRead += bytesRead;
					totalReads++;
				}
		
				// If all bytes were read, write to output file
				if(bytesRead == blocksize)
				{
					bytesWritten = write(outputFD, transferBuffer, bytesRead);
					if(bytesWritten < 0){
						perror("Error writing output file");
						exit(EXIT_FAILURE);
					}
					else{
						totalBytesWritten += bytesWritten;
						totalWrites++;
					}
				}
				// Otherwise assume we have reached the end of the input file and reset
				else
				{
					if(lseek(inputFD, 0, SEEK_SET))
					{
						printf("Error resetting to beginning of file\n");
						exit(EXIT_FAILURE);
					}
					inputFileResets++;
				}
			} while(totalBytesWritten < transfersize);

			// Output some possibly helpful info to make it seem like we were doing stuff
			fprintf(stdout, "Read:    %zd bytes in %d reads\n",
				totalBytesRead, totalReads);
			fprintf(stdout, "Written: %zd bytes in %d writes\n",
				totalBytesWritten, totalWrites);
			fprintf(stdout, "Read input file in %d pass%s\n",
				(inputFileResets + 1), (inputFileResets ? "es" : ""));
			fprintf(stdout, "Processed %zd bytes in blocks of %zd bytes\n",
				transfersize, blocksize);
		
			// Free Buffer
			free(transferBuffer);

			// Close Output File Descriptor
			if(close(outputFD))
			{
				printf("Failed to close output file\n");
				exit(EXIT_FAILURE);
			}

			// Close Input File Descriptor
			if(close(inputFD))
			{
				printf("Failed to close input file\n");
				exit(EXIT_FAILURE);
			}

			exit(EXIT_SUCCESS);
		}
	}
	
	statout_fp = fopen(STATOUT_FILE, "a"); // append to file
	if(statout_fp == NULL)
	{
		printf("Error Opening Output File\n");
	}
	else
	{
		fprintf(statout_fp, "Statistics for I/O bound, policy:%s, num_children:%d\n",
			argv[3], num_children);
	}
	// done forking in parent process...now wait and reap children
	while (1) {
		// wait and collect statistics
		pid = wait4(-1, &status, 0, &r_usage);
		
		if(pid == -1) 
		{
			if(errno == ECHILD) 
			{
				printf("No more children\n");
				break; // no more child processes
			}
		} 
		else 
		{
			if(statout_fp != NULL)
			{
				// write the output file
				fprintf(statout_fp, "PID[%d]: Utime: %ld(s), %ld(us)", pid,
					r_usage.ru_utime.tv_sec, r_usage.ru_utime.tv_usec);
				fprintf(statout_fp, " Stime: %ld(s), %ld(us)", r_usage.ru_stime.tv_sec,
					r_usage.ru_stime.tv_usec);
				fprintf(statout_fp,
					" inblockops:%ld, outblockops:%ld, volcsw:%ld, involcsw:%ld\n",
					r_usage.ru_inblock, r_usage.ru_oublock, r_usage.ru_nvcsw,
					r_usage.ru_nivcsw);
			}
			if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) 
			{
				printf("pid %d failed\n", pid);
				exit(EXIT_FAILURE);
			}
		}
	}
	
	
	if(statout_fp != NULL)
	{
		fclose(statout_fp);
	}
	
    exit(EXIT_SUCCESS);
		
}
