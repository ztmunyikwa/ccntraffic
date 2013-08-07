#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

struct urlrangepair ** ranges;
int totalUrls;
double harmconstant;
struct urlprobpair {
    char *url;
    double prob;
};

struct urlrangepair {
    char *url;
    int range[2];
};

//calculates a modified harmonic series for the pmf of zipf
double harmonic(int n, int alpha)
{
    double answer = 0;
    double counter = 1;

    while (counter <= n) {
        answer = answer + pow((1.0 / counter), alpha);
        counter++;
    }

    return answer;
}

//returns a double, the probability of x occurring with a specified alpha, and n possible outcomes
double pmf(int x, int alpha, int n, double constant)
{
    double answer;
    answer = 1.0 / (pow(x, alpha) * (constant));
    return answer;
}

int closestten(double n)
{
    return (int) pow(10, ceil(log10(n)));
}

//takes in the list of urls and calculates the range that each random uniform number would correspond to which urls
struct urlrangepair ** createurlrangepair2(char ** urlList, int alpha, int urlCount)
{
    harmconstant = harmonic(urlCount, alpha);  //calculates constant needed for probability density function
    int totalbuckets = closestten(1 / pmf(urlCount, alpha, urlCount, harmconstant));
    int low = 0;
    int i;
    ranges = malloc(urlCount * sizeof(struct urlrangepair*));

    for(i = 0; i < urlCount; i++) {
        int rank = i + 1;
        int numbaskets = (int) round(pmf(i + 1, alpha, urlCount, harmconstant) * totalbuckets);
        ranges[i] = malloc(sizeof(struct urlrangepair));
        ranges[i] -> url = urlList[i];
        ranges[i] -> range[0] = low;
        ranges[i] -> range[1] = low + numbaskets - 1;
        low = low + numbaskets;
    }

    return ranges;
}



//takes in the list of urls and calculates the range that each random uniform number would correspond to which urls
struct urlrangepair ** createurlrangepair(struct urlprobpair** probabilities, int urlCount)
{
    int totalbuckets = closestten(1 / probabilities[urlCount - 1] -> prob);
    int low = 0;
    int i;
    struct urlrangepair **ranges = malloc(urlCount * sizeof(struct urlrangepair*));

    for (i = 0; i < urlCount; i++) {
        int numbaskets = (int) round(probabilities[i] -> prob * totalbuckets);
        ranges[i] = malloc(sizeof(struct urlrangepair));
        ranges[i] -> url = probabilities[i] -> url;
        ranges[i] -> range[0] = low;
        ranges[i] -> range[1] = low + numbaskets - 1;
        low = low + numbaskets;
        printf("%s  %d - %d \n", ranges[i] -> url, ranges[i] -> range[0], ranges[i] -> range[1]);
    }

    return ranges;
}

//takes in a urlList and uses pmf to calculate the probability of a given url being selected under the zipf distribution
struct urlprobpair ** calc_probs(char **urlList, int alpha, int urlCount)
{
    int i;
    struct urlprobpair **probabilities = malloc(urlCount * sizeof(struct urlprobpair *));
    harmconstant = harmonic(urlCount, alpha);

    for (i = 0; i < urlCount; i++) {
        int rank = i + 1;
        probabilities[i] =  malloc(sizeof(struct urlprobpair));
        probabilities[i]-> url = urlList[i];
        probabilities[i]-> prob =  pmf(rank, alpha, urlCount, harmconstant);
    }

    for (i = 0; i < urlCount; i++) {
        printf("i = %d url = %s, probability = %f\n", i, probabilities[i] -> url, probabilities[i] -> prob);
    }

    return probabilities;
}

int getLineCount(const char* fileName)
{
    //printf("file is %s", fileName);
    FILE *file = fopen(fileName, "r");

    if(file == NULL) {
        perror("Cannot open the file in getLineCount, exiting.. \n");
        exit(-1);
    }

    int lineCount = 0;
    char line[2000];

    while(fgets(line, sizeof line, file) != NULL) {
        lineCount++;
    }

    fclose(file);
    //printf("Get lineCount executed fine.");
    return lineCount;
}


//some of this taken from existing ccntraffic.c code, within main
//takes a filename and puts each file name into a spot in a chararray
char** filetoarray(const char *fileName)
{
    char ** urlList = NULL;
    int lineCount = getLineCount(fileName);
    printf("There are %d URLs in the file %s\n", lineCount, fileName);
    urlList = malloc(lineCount * sizeof(char*));
    char line[2000];
    int idx = 0;
    int lineIdx = 0;
    //printf("Okay up to here");
    FILE *file = fopen(fileName, "r");

    if (file == NULL) {
        printf("In filetoarray, cannot open file %s, exiting...\n", fileName);
        exit(-1);
    }

    printf("%d", lineCount);

    while(fgets(line, sizeof line, file) != NULL) {
        if(lineIdx >= 0 && lineIdx < lineCount) {
            urlList[idx] = malloc((strlen(line) + 1) * sizeof(char));
            strcpy(urlList[idx], line);
            urlList[idx][strlen(line) - 1] = '\0';
            //printf("%d %s\n", idx, urlList[idx]);
            idx++;
        }

        lineIdx++;
    }

    fclose(file);
    return urlList;
}


//performs a binary search of an array of structs (urls and their corresponding ranges) to see if the
//falls within any of the ranges
int searcharray(int randint, int alpha, struct urlrangepair** ranges, int count, double partition)
{
    int low = 0;
    int high = count - 1;
    int round = 0;
    int mid = 0;

    while (low <= high) {
        if (round == 0 && partition >= 0) {
            double cdf = 0;
            int temp = 0;

            while (cdf < partition && fabs(cdf - partition) > 0.05) {
                cdf = cdf + pmf(temp + 1, alpha, totalUrls, harmconstant);
                //printf("current i = %d cdf = %f, pmf = %f, temp = %d\n", temp, cdf, pmf(temp+1, 1, totalUrls, harmconstant), temp);
                temp = temp + 1;
            }

            mid  = temp;
            round ++;

        } else {
            mid = ((high + low) / 2);
            printf("mid = %d", mid);
            round ++;
        }

        if (ranges[mid]-> range[0] > randint) {
            high = mid - 1;

        } else if(ranges[mid] -> range[1] < randint) {
            low = mid + 1;

        } else {
            return mid;
        }
    }

    return 0;
}

int searcharray2(int randint, struct urlrangepair** ranges, int count, double partition)
{
    int low = 0;
    int high = count - 1;
    int round = 0;
    int mid = 0;

    while (low <= high) {
        if (round == 0 && partition >= 0) {
            double cdf = 0;
            int temp = 0;

            while (cdf < partition && fabs(cdf - partition) > 0.05) {
                cdf = cdf + pmf(temp + 1, 1, totalUrls, harmconstant);
                //printf("current i = %d cdf = %f, pmf = %f, temp = %d\n", temp, cdf, pmf(temp+1, 1, totalUrls,     harmconstant), temp);
                temp = temp + 1;
            }

            mid  = temp;
            round ++;

        } else {
            mid = ((high + low) / 2);
            round ++;
            //printf("mid = %d round = %d", mid, round);
        }

        if (mid  > randint) {
            high = mid - 1;

        } else if(mid  < randint) {
            low = mid + 1;

        } else {
            //printf("final numlookup count = %d", round);
            return round;
        }
    }

    return 0;
}


void zipfinit(char *urlFile, int alpha)
{
    int i;
    char** allUrls = filetoarray(urlFile);
    struct urlprobpair ** probabilities = calc_probs(allUrls, alpha, totalUrls);
    //free(allUrls);
    printf("totalurls == %d", totalUrls);
    ranges = createurlrangepair(probabilities, totalUrls); //alpha equals 1
}

int zipfrand(int lineCount, int alpha, double partition)
{
    int randint = rand() % ranges[lineCount - 1] -> range[1];
    int urlIndex = searcharray(randint, alpha, ranges, lineCount, partition);
    //packetssent[urlIndex] ++;
    return urlIndex;
}



//generates random urls within the zipf distribution
void runzipf(char * fileName, int samplesize, int alpha, double partition)
{
    clock_t begin;
    clock_t end;
    double time_spent;
    double avg_lookup;
    int totalurls = getLineCount(fileName);
    int i;
    int j;
    int k;
    totalUrls = totalurls;
    zipfinit(fileName, alpha);
    //struct urlprobpair ** probabilities = calc_probs(urlList, alpha, totalurls);
    //struct urlrangepair ** ranges = createurlrangepair(probabilities, totalurls);
    int countarray[totalurls];
    srand(time(NULL));
    FILE *fp;
    FILE *fp2;
    char ch;
    fp = fopen("results.txt", "w");

    for(i = 0; i < totalurls; i++) {
        countarray[i] = 0;
    }

    begin = clock();

    for (j = 0; j < samplesize; j++) {
        int urlIndex = zipfrand(totalUrls, alpha, partition);
        char * result = malloc(sizeof(ranges[urlIndex] -> url));
        result = ranges[urlIndex] -> url;
        countarray[urlIndex] ++;
        //fprintf(fp, "%s/", result);
        //for (i = 0; i< countarray[urlIndex]; i++) {
        //fprintf(fp, "%d", i);
        //}
        //fputs("\n", fp);
        printf("%s\n", result);
    }

    end = clock();
    time_spent = (double) (end - begin) / CLOCKS_PER_SEC;
    avg_lookup = (double) time_spent / samplesize;

    for (k = 0; k < totalurls; k++) {
        //fprintf(fp, "Url: %s   Number of occurrences: %d  Percentage of sample: %f\n", ranges[k]-> url,countarray[k], 100.0*(countarray[k]/samplesize));
        fprintf(fp, "%d\n", countarray[k]);
    }

    fclose(fp);
    fp2 = fopen("timings.txt", "a");
    fprintf(fp2, "partition = %f time_spent = %f avg lookup = %f\n", partition, time_spent, avg_lookup);
    fclose(fp2);
}

double calc_expected_value(char *fileName, int samplesize, int alpha, double partition)
{
    int i;
    printf("%s", fileName);
    int totalurls = getLineCount(fileName);
    int j;
    int k;
    totalUrls = totalurls;
    char** allUrls = filetoarray(fileName);
    struct urlprobpair ** probabilities = calc_probs(allUrls, 1, totalUrls);
    //free(allUrls);
    printf("totalurls == %d", totalUrls);
    ranges = createurlrangepair(probabilities, totalUrls);
    int numlookups;
    double expvalue = 0;

    for (i = 0 ; i < totalUrls; i++) {
        numlookups = searcharray2(i, ranges, totalUrls, partition);
        //printf("index = %d; numlookups = %d,%d ", i, numlookups, searcharray2(i, ranges, totalUrls, partition));
        //printf("prob %f times num lookups %d = %f\n", probabilities[i] -> prob, numlookups, (probabilities[i] -> prob)*numlookups);
        expvalue = expvalue + (probabilities[i] -> prob) * numlookups;
        //printf("%f\n", expvalue);
    }

    return expvalue;
}

int main (int argc, char** argv)
{
    if (argv[1] == NULL || argv[2] == NULL || argv[3] == NULL) {
        printf("Please format your arguments in this format: ./zipf [filename] [alpha] [sample size] \n");
        exit;
    }

    char * fileName = argv[1];
    int alpha = atoi(argv[2]);
    int samplesize = atoi(argv[3]);
    double partition = atof(argv[4]);
    printf("%s %d %d %f", fileName, alpha, samplesize, partition);
    double i;
    //createurlrangepair(calc_probs(filetoarray(fileName), alpha, getLineCount(fileName)), getLineCount(fileName));
    runzipf(fileName, samplesize, alpha, partition);
    //for (i =0 ; i<1; i = i + 0.1) {
    //printf("\ni = %f; expvalue = %f\n", i, calc_expected_value(fileName, samplesize, alpha, i));
    //}
}

