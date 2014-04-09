/* mixed.c
 * Author: Conrad Hougen
 * Project: CSCI 3753 Programming Assignment 3
 * Description:
 * 	This file implements a mix of CPU and I/O operations
 * 	for benchmarking different Linux scheduler policies.
 * 	Specifically, it computes and approximation of pi using
 * 	a statistical method, then writes intermediate results
 *  to data files.  The pi computation is CPU intensive while
 * 	the file writes are I/O intensive.  The data files can be
 *  used as input to the I/O intensive rw.c program.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define MAXFILENAMELENGTH 80
#define RADIUS (RAND_MAX / 2)
#define MAX_CHILDREN 200
#define DEFAULT_OUTPUTFILENAMEBASE "output/dat"
char STATOUT_FILE[MAXFILENAMELENGTH];

inline double dist(double x0, double y0, double x1, double y1){
    return sqrt(pow((x1-x0),2) + pow((y1-y0),2));
}

inline double zeroDist(double x, double y){
    return dist(0, 0, x, y);
}

int main(int argc, char* argv[]){

    long i;
	int j;
	int rv;
    long iterations;
    struct sched_param param;
    int policy;
    double x, y;
    double inCircle = 0.0;
    double inSquare = 0.0;
    double pCircle = 0.0;
    double piCalc = 0.0;
    int num_children;
    pid_t pid;
    int status;
    FILE *statout_fp = NULL;
    struct rusage r_usage; // struct to collect statistics for each forked process
    FILE *output_fp;
    char outputFilename[MAXFILENAMELENGTH];
    char outputFilenameBase[MAXFILENAMELENGTH];
    
    
    // check that the correct number of cmdline args are used
    if(argc != 5){
		printf("argc = %d\n", argc);
		printf("Needs 4 args: <num iterations> <policy> <num processes> <trial number>\n");
		exit(EXIT_FAILURE);
    }
    // Set iterations
	iterations = atol(argv[1]);
	if(iterations < 1){
	    printf("Bad iterations value\n");
	    exit(EXIT_FAILURE);
	}
    // Set policy
	if(!strcmp(argv[2], "SCHED_OTHER")){
	    policy = SCHED_OTHER;
	}
	else if(!strcmp(argv[2], "SCHED_FIFO")){
	    policy = SCHED_FIFO;
	}
	else if(!strcmp(argv[2], "SCHED_RR")){
	    policy = SCHED_RR;
	}
	else{
	    printf("Unhandled scheduling policy\n");
	    exit(EXIT_FAILURE);
	}
    
    // Set number of child processes to fork
    num_children = atol(argv[3]);
    if(num_children < 1 || num_children > MAX_CHILDREN)
    {
		printf("The number of child processes must be in the range [1,999]\n");
		exit(EXIT_FAILURE); 
	}
	
	// Get the output file name corresponding to trial number
	j = atol(argv[4]);
	sprintf(STATOUT_FILE, "test_results/test%d.txt", j);
	
    // Set process to max priority for given scheduler
    param.sched_priority = sched_get_priority_max(policy);
    
    if(strnlen(DEFAULT_OUTPUTFILENAMEBASE, MAXFILENAMELENGTH) >= MAXFILENAMELENGTH){
	    fprintf(stderr, "Default output filename base too long\n");
	    exit(EXIT_FAILURE);
	}
	strncpy(outputFilenameBase, DEFAULT_OUTPUTFILENAMEBASE, MAXFILENAMELENGTH);
    
    // Set new scheduler policy
    fprintf(stdout, "Current Scheduling Policy: %d\n", sched_getscheduler(0));
    fprintf(stdout, "Setting Scheduling Policy to: %d\n", policy);
    if(sched_setscheduler(0, policy, &param)){
		perror("Error setting scheduler policy");
		exit(EXIT_FAILURE);
    }
    fprintf(stdout, "New Scheduling Policy: %d\n", sched_getscheduler(0));
    
    
    // Fork num_children child processes to test mixed process scheduling
	for(j=0; j<num_children; ++j)
	{
		if((pid = fork()) == -1)
		{
			printf("Fork failed with j = %d\n", j);
			exit(EXIT_FAILURE);
		}
		else if(pid == 0)
		{
			// Open output data file for writing with standard permissions
			rv = snprintf(outputFilename, MAXFILENAMELENGTH, "%s-%d",
				outputFilenameBase, j+1);    
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
			if((output_fp = fopen(outputFilename, "w")) == NULL)
			{
				printf("Failed to open output file\n");
				exit(EXIT_FAILURE);
			}
			
			// Calculate pi using statistical method
			for(i=0; i<iterations; ++i)
			{
				// CPU operation
				x = (random() % (RADIUS * 2)) - RADIUS;
				fprintf(output_fp, "%8.7f", x);
				y = (random() % (RADIUS * 2)) - RADIUS;
				fprintf(output_fp, "%8.7f", y);
				if(zeroDist(x,y) < RADIUS)
				{
					inCircle++;
					fprintf(output_fp, "%8.7f", inCircle);
				}
				inSquare++;
				pCircle = inCircle/inSquare;
				piCalc = pCircle * 4.0;
				
				// write result to file (I/O operation)
				if(fprintf(output_fp, "%8.7f", piCalc) < 0){
					printf("Error writing output file");
					exit(EXIT_FAILURE);
				}
			}

			// Print result
			fprintf(stdout, "pi = %f\n", piCalc);
			
			// Close Output File Descriptor
			fclose(output_fp);
			
			// child process done, so exit
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
		fprintf(statout_fp, "Statistics for Mixed, policy:%s, num_children:%d\n",
			argv[2], num_children);
	}
	// done forking in parent process...now wait and reap children
	while (1) {
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
