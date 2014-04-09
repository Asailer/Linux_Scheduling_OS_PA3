Conrad Hougen
Programming Assignment 3
CSCI 3753 - Operating Systems
Professor Shivakant Mishra

Input files are included in the input folder for rw.c to read from.  These
files are 1MB each, so the BYTESTOREAD should not be greater than 1MB.
Building and running the code should be as simple as entering this
directory in the terminal and typing (without the line prompt):

>>bash VMtestscript

This should clean the folder, then build and run all 27 tests over as
many trials as you would like (change the while loop limit in VMtestscript
as necessary).  Output data will be generated in separate text files
per trial in the test_results folder.  Other parameters that can be
changed are the number of iterations for the CPU-bound and mixed-type
processes (called ITERATIONS and MIXED_ITER respectively), and the
total bytes to transfer and transfer blocksize for the I/O-bound
processes (called BYTESTOCOPY and BLOCKSIZE respectively).  Each of these
parameters is defined in VMtestscript.  Of course, it is also possible to
run individual tests, so look at the test script for examples.