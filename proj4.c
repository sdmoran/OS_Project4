#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <semaphore.h>

#define MAXTHREAD 16
#define MASTER 0
#define RANGE 2
#define GENDONE 3

sem_t *ssem;
sem_t *rsem;

char *pchFile;
char *str;
int fileSize;
struct timeval time1;
struct timeval time2;

struct msg {
	int iSender;
	int type;
	int value1;
	int value2;
};

struct msg mailboxes[MAXTHREAD + 1];

void SendMsg(int iTo, struct msg *pMsg) {
	sem_wait(&ssem[iTo]); //block and wait for mailbox to become vacant
	mailboxes[iTo] = *pMsg;
	sem_post(&rsem[iTo]); //frees up rsem
}

void RecvMsg(int iFrom, struct msg *pMsg) {
	sem_wait(&rsem[iFrom]);
	*pMsg = mailboxes[iFrom];
	sem_post(&ssem[iFrom]); //frees up ssem for another thread
}

int BUFSIZE;

void compareTo(char c, int* index, int* count, int* reading);

void *startPoint(void *param) {
	printf("Successfully reached test function\n");
	return 0;
}

void *searchChunk(int threadNum) {
	int localCount = 0;	
	struct msg inMessage;
	struct msg outMessage;	
	RecvMsg(threadNum, &inMessage);
	int start = inMessage.value1;
	int stop = inMessage.value2;
	for(int i = start; i <= stop; i++) {
	//checks if current char in buf matches first char of search string		
		if(str[0] == pchFile[i]) {
			int isSame = 1;
			//iterates through search string comparing to next several chars in buf
			for(int j = 0; j < strlen(str); j++) {	
				if(str[j] != pchFile[i+j]) {
					isSame = 0;
				}
			}
			if(isSame)		
				localCount++;
		}
	}

	outMessage.iSender = threadNum;
	outMessage.type = 1;
	outMessage.value1 = localCount;
	outMessage.value2 = localCount;
	SendMsg(MASTER, &outMessage);
	return 0;
}

void mmapWithThreads(int fdIn, char* str, int threads) {
	struct stat sb;
	int count = 0;
	pthread_t *threadArr;

	rsem = malloc(sizeof(sem_t) * (threads + 1));
	ssem = malloc(sizeof(sem_t) * (threads + 1));

	for(int i = 0; i < threads + 1; i++) {
		if(sem_init(&rsem[i], 0, 0) < 0) {
			printf("rsem failed");
			return 1;
		}
		if(sem_init(&ssem[i], 0, 1) < 0) {
			printf("ssem failed");
			return 1;	
		}
		int j;
		int k;
		sem_getvalue(&rsem[i], &j);
		sem_getvalue(&ssem[i], &k);
	}

	if(fstat(fdIn, &sb) < 0) {
		perror("Could not stat file to obtain its size");
		exit(1);
	}

	if((pchFile = (char *) mmap (NULL, sb.st_size, PROT_READ, MAP_SHARED, fdIn, 0)) == (char *) -1) {
		perror("Could not mmap file");
	}
	printf("File size: %d bytes\n", sb.st_size);

	//Divide up work between threads:
	int eachThread = (int) sb.st_size / threads;
	int lastThread = eachThread + strlen(pchFile) % eachThread;

	threadArr = malloc(sizeof(pthread_t) * threads);
	for(int i = 0; i < threads; i++) {
		if(pthread_create(&threadArr[i], NULL, searchChunk, i + 1)) {
		printf("ERROR");
		}
	}
	
	struct msg msg0, msg1;

	for(int i = 0; i < threads; i++) {
		msg0.iSender = i + 1;
		msg0.type = RANGE;
		msg0.value1 = eachThread * i + 1;
		msg0.value2 = eachThread * i + eachThread;
		//if this is the final thread, may need to add a few more numbers than the others
		if(i + 1 == threads) {
			msg0.value2 = eachThread * i + lastThread;
		}
		SendMsg(i + 1, &msg0);
		RecvMsg(MASTER, &msg1);
		count += msg1.value1;
	}

	for(int i = 0; i < threads; i++) {
		pthread_join(threadArr[i], NULL);
	}
	printf("Occurences of search string: %d\n", count);

	if(munmap(pchFile, sb.st_size) < 0) {
		perror("Could not unmap memory!");
		exit(1);
	}
}

int main(int argc, char* argv[]) {
	struct rusage usage;
	gettimeofday(&time1, NULL);
	int threads;
	int fdIn;
	int doMmap, mmapthreads;

	if (argc < 2) {
		fdIn = 0;  /* just read from stdin */
	}

	else if ((fdIn = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "file open\n");
		exit(1);
	}

	if(argc < 3) {
		printf("Not enough parameters!\nExample usage:");
		printf(" ./proj4 srcfile searchstring [size|mmap]\n");
		return 0;
	}

	str = argv[2];
	int cnt, i;
	int count = 0;

	doMmap = 0;
	mmapthreads = 0;

	if(strncmp(argv[3], "mmap", 3) == 0) {
		printf("Method used: MMAP\n");
		doMmap = 1;
	}
	else if(strncmp(argv[3], "p", 1) == 0) {			
		mmapthreads = 1;		
		char* num = argv[3] + 1;			
		threads = atoi(num);
		if(threads > 16) {
			printf("Too many threads! Using max (16).\n");
			threads = 16;
		}
		printf("Method used: MMAP with %d threads\n", threads);
		mmapWithThreads(fdIn, str, threads);
	}

	else {
		printf("Method used: SIZE\n");
		BUFSIZE = atoi(argv[3]);
	}
	char buf[BUFSIZE];

	if(doMmap) {
		char *pchFile;
		struct stat sb;

	if(fstat(fdIn, &sb) < 0) {
		perror("Could not stat file to obtain its size");
		return(1);
	}

	if((pchFile = (char *) mmap (NULL, sb.st_size, PROT_READ, MAP_SHARED, fdIn, 0)) == (char *) -1) {
		perror("Could not mmap file");
	}
	
	fileSize = sb.st_size;

	for(int i = 0; i < sb.st_size; i++) {
	//checks if current char in buf matches first char of search string		
	if(str[0] == pchFile[i]) {
		int isSame = 1;
		//iterates through search string comparing to next several chars in buf
		for(int j = 0; j < strlen(str); j++) {
			//printf("str: %c, file: %c\n", str[j], pchFile[i+j]);			
			if(str[j] != pchFile[i+j]) {
				isSame = 0;
			}
		}
		if(isSame)		
			count++;
	}
}

//process guacamole....
if(munmap(pchFile, sb.st_size) < 0) {
		perror("Could not unmap memory!");
		exit(1);
}
	}

//BIGCHONK MULTITHREAD RRRREEEEEEE


/*PLAN: Have a buffer same length as str. Read through input.
	- On first letter of str == first letter of input
		Start reading into buffer
	Stop if:
		Characters no longer match
		Buffer gets full and it all matches
	CONTINUE READING WHERE BUF LEFT OFF if reach end of chunk or end of thread allotment
	
*/

	/*2 cases to handle:
		- Chunk size is larger than the length of the string
			- In this case, just need to check the last strlen(str) characters of previous
			  buf and first strlen(str) chars of next buf
		- Chunk size is less than the length of the string
			- Read into a buffer of length str - when full, compare? Then flush and repeat
		- Will that even work tho??
		*/

	else {
	//String to keep track of boundaries between chunks
	int index = 0;
	char tempBuf[strlen(str)];
	int reading = 0;
    //iterate through input
    while ((cnt = read(fdIn, buf, BUFSIZE)) > 0) {
	fileSize += cnt;
	//Iterates thru current buf
	for(int i = 0; i < cnt; i++) {
		//checks if current char in buf matches first char of search string
		//OR if program is currently reading a string started in another chunk
		//printf("%c", buf[i]);	
		if(str[0] == buf[i] || reading) {
			if(!reading) {
			index = 0;
			}			
			//printf("HIT! str[0]: %c buf[%d]: %c\n", str[0], i, buf[i]);
			if(strlen(str) == 1) {
				count++;
			}
			else {	
				reading = 1;
				compareTo(buf[i], &index, &count, &reading);
			}	
		}
	}
    }
    if (fdIn > 0)
        close(fdIn);

}

	//iterate through memory
	if(!mmapthreads)
		printf("\nOccurrences of search string: %d\n", count);
	getrusage(RUSAGE_SELF, &usage);
	gettimeofday(&time2, NULL);
	//suseconds_t curTime = time2.tv_usec - time1.tv_usec;
	printf("File size: %d bytes\n", fileSize);
	printf("Time elapsed (microseconds): %lu\n", time2.tv_usec - time1.tv_usec);
	printf("Page Faults: %lu\n", usage.ru_majflt);

}

void compareTo(char c, int* index, int* count, int* reading) {	
	//printf("COMPARETO:\n");
	//printf("c: %c, str[index]: %c\n", c, str[*index]);	
	if(c == str[*index]) {
		if(*index == strlen(str) - 1) {			
			*count += 1;
			*index = 0;
			*reading = 0;
		}
		if(reading != 0) {
			*index += 1;
		}
	}
	else {
		*index = 0;
		*reading = 0;
	}
}
