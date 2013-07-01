#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct urlprobpair 
{
	char *url;
	double prob;
}; 

struct urlrangepair
{
	char *url;
	int range[2];
};

//calculates a modified harmonic series for the pmf of zipf
double harmonic(int n, int alpha) {
	double answer = 0;
	double counter = 1;
	while (counter <= n) 
	{
		
		answer = answer + pow((1.0/counter), alpha);
		
		counter++;
	}
	return answer;
}

//returns a double, the probability of x occurring with a specified alpha, and n possible outcomes
double pmf(int x, int alpha, int n) {
	double answer;
	
	answer = 1.0/ (pow(x, alpha) * (harmonic(n, alpha)));
	return answer;
}

int closestten(double n) {
	return (int) pow(10, ceil(log10(n)));
}


//takes in the list of urls and calculates the range that each random uniform number would correspond to which urls
struct urlrangepair ** createurlrangepair(struct urlprobpair** probabilities, int urlCount) {
	int totalbuckets = closestten(1/ probabilities[urlCount-1] -> prob);
	int low = 0;
	int i;

	struct urlrangepair **ranges = malloc(urlCount * sizeof(struct urlrangepair*));

	for (i = 0; i<urlCount; i++) {
		int numbaskets = (int) round(probabilities[i] -> prob * totalbuckets);
		ranges[i] = malloc(sizeof(struct urlrangepair));
		ranges[i] -> url = probabilities[i] -> url;
		ranges[i] -> range[0] = low;
		ranges[i] -> range[1] = low + numbaskets-1;

		low = low + numbaskets;
		//printf("%s  %d - %d \n", ranges[i] -> url, ranges[i] -> range[0], ranges[i] -> range[1]);

	}


	return ranges;
}
 
 //takes in a urlList and uses pmf to calculate the probability of a given url being selected under the zipf distribution
struct urlprobpair ** calc_probs(char **urlList, int alpha, int urlCount) {
	int i; 
	
	struct urlprobpair **probabilities = malloc(urlCount * sizeof(struct urlprobpair *));
	

	for (i = 0; i< urlCount; i++) {
		int rank = i+1; 
		probabilities[i] =  malloc(sizeof(struct urlprobpair));
		probabilities[i]-> url = urlList[i];

		probabilities[i]-> prob =  pmf(rank, alpha, urlCount);
		

	}
	
	return probabilities;
	



}

int getLineCount(const char* fileName) {

	FILE *file = fopen(fileName, "r");

	if(file == NULL) {
		perror("Cannot open the file in getLineCount, exiting.. \n");
		exit(-1);
	}

	int lineCount = 0;
	char line[2000];
	while(fgets(line, sizeof line, file) != NULL){
		lineCount++;
	}

	fclose(file);
	return lineCount;
}


 //some of this taken from existing ccntraffic.c code, within main
//takes a filename and puts each file name into a spot in a chararray
char** filetoarray(const char *fileName) {
	char ** urlList = NULL;
	int lineCount = getLineCount(fileName);

	printf("There are %d URLs in the file %s\n", lineCount, fileName);

	urlList = malloc(lineCount * sizeof(char*));

	char line[2000];

	int idx = 0;
	int lineIdx = 0; 

	FILE *file = fopen(fileName, "r");
	if (file == NULL) {
		printf("Cannot open file %s, exiting...\n", fileName);
		exit(-1);
	}

	while(fgets(line, sizeof line, file) != NULL) {
		if(lineIdx >= 0 && lineIdx < lineCount){
			urlList[idx] = malloc((strlen(line)) * sizeof(char));
			strcpy(urlList[idx], line);	
			urlList[idx][strlen(line)-1] = '\0';

			idx++;
		}
		lineIdx;

	}
	

	return urlList; 


}

//performs a binary search of an array of structs (urls and their corresponding ranges) to see if the 
//falls within any of the ranges
int searcharray(int randint, struct urlrangepair** ranges, int count) {
	int low = 0;
	int high = count -1;

	int mid = 0;

	while (low <= high) {
		mid = ((high + low)/ 2);
		if (ranges[mid]-> range[0] > randint) {
			high = mid -1;
		} else if(ranges[mid] -> range[1] < randint) {
			low = mid + 1;
		} else {
			
			return mid;

		}
	}
	return 0;

}

//generates random urls within the zipf distribution
void runzipf(char * fileName, int samplesize, int alpha) {
	int totalurls = getLineCount(fileName); int i;int j; int k;


	char** urlList = filetoarray(fileName);
	struct urlprobpair ** probabilities = calc_probs(urlList, alpha, totalurls);
	struct urlrangepair ** ranges = createurlrangepair(probabilities, totalurls);
	int countarray[totalurls];
	srand(time(NULL));
	FILE *fp;
	char ch;

	fp = fopen("results.txt", "w");
	

	for(i = 0; i<totalurls; i++) {
		countarray[i] =0;
	}

	for (j = 0; j< samplesize; j++) {
		int randint = rand()%ranges[totalurls-1]->range[1];
		char * result = malloc(sizeof(ranges[searcharray(randint, ranges, totalurls)] -> url));
		
		result = ranges[searcharray(randint, ranges, totalurls)] -> url;
		countarray[searcharray(randint, ranges, totalurls)] ++;
		fputs(result, fp);
		fputs("\n", fp);

		printf("%s\n", result);
	}

	for (k = 0; k<totalurls; k++) {
		fprintf(fp, "Url: %s   Number of occurrences: %d  Percentage of sample: %f\n", ranges[k]-> url,countarray[k], 100.0*(countarray[k]/samplesize));
	}
}


int main (int argc, char** argv) {
	if (argv[1] == NULL || argv[2] == NULL || argv[3] == NULL){
		printf("Please format your arguments in this format: ./zipf [filename] [alpha] [sample size] \n");
		exit;
	}
	char * fileName = argv[1];


	int alpha = atoi(argv[2]);


	int samplesize = atoi(argv[3]);


	runzipf(fileName, samplesize, alpha);
}
