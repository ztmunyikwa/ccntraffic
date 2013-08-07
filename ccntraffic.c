/**
 * @file ccntraffic.c 0.0.1
 * Derived from ccncatchunks2.c
 *
 * A WUSTL CCNx command-line utility.
 * Haowei Yuan (hyuan@wustl.edu)
 *
 * Copyright (C) 2011 Washington University in St. Louis
 *
 * 9/15/2011
 *
 * This work is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This work is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/** TODO:
 * 0. Log statistics.
 * 1. Enable Zifp distribution support
 * 2. Fix line[2000], should be a variable
 */



#include <sys/time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <ccn/ccn.h>
#include <ccn/charbuf.h>
#include <ccn/schedule.h>
#include <ccn/uri.h>
#include <time.h>

#define PIPELIMIT 10000

#define GOT_HERE() (void)((__LINE__))



int intereststosend;
int *packetssent;
int *packetsreceived;
char * distribution = 1;
struct urlrangepair ** ranges;
int totalUrls;
clock_t begin, end;
int hmcconstant;

//Return the number of lines/URLs in a file
int getLineCount(const char* fileName)
{
    FILE *file = fopen(fileName, "r");
    printf("In getlinecount, file is open");

    if(file == NULL) {
        perror("Cannot open the file in getLineCount, exiting..\n");
        exit(-1);
    }

    int lineCount = 0;
    char line[2000];	//TODO: figure out the size of the line

    while(fgets(line, sizeof line, file) != NULL) {
        lineCount++;
    }

    fclose(file);
    return lineCount;
}


struct ooodata {
    struct ccn_closure closure;     /* closure per slot */
    unsigned char *raw_data;        /* content that has arrived out-of-order */
    size_t raw_data_size;           /* its size (plus 1) in bytes */
};

struct mydata {
    struct ccn *h;
    int allow_stale;
    int use_decimal;

    struct ccn_charbuf *name;
    struct ccn_charbuf *templ;
    struct excludestuff *excl;
    struct ccn_schedule *sched;
    struct ccn_scheduled_event *report;

    intmax_t interests_sent;
    intmax_t pkts_recvd;
    intmax_t co_bytes_recvd;
    intmax_t delivered;
    intmax_t delivered_bytes;
    intmax_t timeouts;
    intmax_t unverified;
    intmax_t dups;

    struct ooodata *ooo;
};

struct urlprobpair {
    char *url;
    double prob;
};

struct urlrangepair {
    char *url;
    int range[2];
};

static void
usage(const char *progname)
{
    fprintf(stderr,
            "%s [-f URI file path] [-n flying] [-s starting URI index]\n"
            "   Send multiple interests at one time\n"
            "	ccndelphi program is preferred to be used as the NDN server\n"
            "   -f - specify the location of the URI file\n"
            "   -n - specify the number of concurrent interests, i.e., the number of queried URIs, default value = 100\n"
            "   -i - specify the number of total interests sent before program termination\n"
            "   -s - specify the starting point of the requested interest. Default value = 0. With the support of this option, multiple machines can share the same URI file but query differet portion of the URIs\n",
            progname);
    exit(1);
}


struct ccn_charbuf *
make_template(struct mydata *md)
{
    struct ccn_charbuf *templ = ccn_charbuf_create();
    ccn_charbuf_append_tt(templ, CCN_DTAG_Interest, CCN_DTAG);
    ccn_charbuf_append_tt(templ, CCN_DTAG_Name, CCN_DTAG);
    ccn_charbuf_append_closer(templ); /* </Name> */
    // XXX - use pubid if possible
    ccn_charbuf_append_tt(templ, CCN_DTAG_MaxSuffixComponents, CCN_DTAG);
    ccnb_append_number(templ, 1);
    ccn_charbuf_append_closer(templ); /* </MaxSuffixComponents> */

    if (md->allow_stale) {
        ccn_charbuf_append_tt(templ, CCN_DTAG_AnswerOriginKind, CCN_DTAG);
        ccnb_append_number(templ, CCN_AOK_DEFAULT | CCN_AOK_STALE);
        ccn_charbuf_append_closer(templ); /* </AnswerOriginKind> */
    }

    ccn_charbuf_append_closer(templ); /* </Interest> */
    return(templ);
}


//=========================================================================
//= Multiplicative LCG for generating uniform(0.0, 1.0) random numbers    =
//=   - x_n = 7^5*x_(n-1)mod(2^31 - 1)                                    =
//=   - With x seeded to 1 the 10000th x value should be 1043618065       =
//=   - From R. Jain, "The Art of Computer Systems Performance Analysis," =
//=     John Wiley & Sons, 1991. (Page 443, Figure 26.2)                  =
//=========================================================================
double rand_val(int seed)
{
    const long  a =      16807;  // Multiplier
    const long  m = 2147483647;  // Modulus
    const long  q =     127773;  // m div a
    const long  r =       2836;  // m mod a
    static long x;               // Random int value
    long        x_div_q;         // x divided by q
    long        x_mod_q;         // x modulo q
    long        x_new;           // New x value

    // Set the seed if argument is non-zero and then return zero
    if (seed > 0) {
        x = seed;
//    return(0.0);
    }

    printf("x = %ld \n", x);
    // RNG using integer arithmetic
    x_div_q = x / q;
    x_mod_q = x % q;
    x_new = (a * x_mod_q) - (r * x_div_q);

    if (x_new > 0)
        x = x_new;

    else
        x = x_new + m;

    // Return a random value between 0.0 and 1.0
    return((double) x / m);
}

//calculates a modified harmonic series for the pmf of zipf
double harmonic(int n, double alpha)
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
double pmf(int x, double alpha, int n, double constant)
{
    double answer;
    answer = 1.0 / (pow(x, alpha) * (constant));
    return answer;
}

int closestten(double n)
{
    return (int) pow(10, ceil(log10(n)));
}



/** This section calculates the harmonic constant, because it is the first instance where the hmcconstant will be used
*Then, uses the parematers passed to it and the harmonic constant to create the range table. 
*/
struct urlrangepair ** createurlrangepair2(char ** urlList, double alpha, int urlCount)
{
    hmcconstant = harmonic(urlCount, alpha);
    int totalbuckets = closestten(1 / pmf(urlCount, alpha, urlCount, hmcconstant));
    int low = 0;
    int i;
    struct urlrangepair ** ranges = malloc(urlCount * sizeof(struct urlrangepair*));

    for(i = 0; i < urlCount; i++) {
        int rank = i + 1;
        int numbaskets = (int) round(pmf(i + 1, alpha, urlCount, hmcconstant) * totalbuckets);
        ranges[i] = malloc(sizeof(struct urlrangepair));
        ranges[i] -> url = urlList[i];
        ranges[i] -> range[0] = low;
        ranges[i] -> range[1] = low + numbaskets - 1;
        low = low + numbaskets;
    }

    return ranges;
}


//originalcreateurlrangepairfunction that utilizes the array of probabilities to calculate ragnges 
struct urlrangepair ** createurlrangepair(struct urlprobpair ** probabilities, int urlCount)
{
    int  totalbuckets = closestten(1 / probabilities[urlCount - 1] -> prob);
    int low = 0;
    int i;
    struct urlrangepair **ranges = malloc(urlCount * sizeof(struct urlrangepair*));

    //printf("in create urlrangepair");
    for (i = 0; i < urlCount; i++) {
        int numbaskets = (int) round(probabilities[i] -> prob * totalbuckets);
        ranges[i] = malloc(sizeof(struct urlrangepair));
        ranges[i] -> url = probabilities[i] -> url;
        //printf("calcranges %d =%s", i, ranges[i] -> url);
        ranges[i] -> range[0] = low;
        ranges[i] -> range[1] = low + numbaskets - 1;
        low = low + numbaskets;
    }

    return ranges;
}

//takes in a urlList and uses pmf to calculate the probability of a given url being selected under the zipf distribution
/*struct urlprobpair ** calc_probs(char **urlList, double alpha, int urlCount) {
	int i;

	struct urlprobpair **probabilities = malloc(urlCount * sizeof(struct urlprobpair *));


	for (i = 0; i< urlCount; i++) {
		int rank = i+1;
		probabilities[i] =  malloc(sizeof(struct urlprobpair));
		probabilities[i]-> url = urlList[i];
		printf("calcprobs %d = %s",i, probabilities[i]-> url);
		probabilities[i]-> prob =  pmf(rank, alpha, urlCount);


	}
	//printf("created all probabilities");

	return probabilities;


}
*/

//converts the contents of a file into an array of pointers to char arrays 
char** filetoarray(const char *fileName)
{
    char ** urlList = NULL;
    int lineCount = getLineCount(fileName);
    //printf("There are %d URLs in the file %s\n", lineCount, fileName);
    urlList = malloc(lineCount * sizeof(char*));
    char line[2000];
    int idx = 0;
    int lineIdx = 0;
    FILE *file = fopen(fileName, "r");

    if (file == NULL) {
        printf("In filetoarray, cannot open file %s, exiting...\n", fileName);
        exit(-1);
    }

    while(fgets(line, sizeof line, file) != NULL) {
        if(lineIdx >= 0 && lineIdx < lineCount) {
            urlList[idx] = malloc((strlen(line) + 1) * sizeof(char));
            strcpy(urlList[idx], line);
            urlList[idx][strlen(line) - 1] = '\0';
            idx++;
        }

        lineIdx++;
    }

    return urlList;
}

//search the range table for a random integer and return the url that is mapped to it  
int searcharray(int randint, struct urlrangepair** ranges, int count)
{
    int low = 0;
    int high = count - 1;
    int mid = 0;

    while (low <= high) {
        mid = ((high + low) / 2);

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


//return the index of an url in the rangetable, when given a string corresponding to the url
int findurlIndex(char * randurl, struct urlrangepair** ranges, int count)
{
    int urlIndex = -1;
    int i;

    for (i = 0; i < count; i++) {
        if (strncmp(ranges[i] -> url + 6, randurl, 100) == 0 ) {
            urlIndex = i;
            return urlIndex;
        }
    }

    return urlIndex;
}

//initialize the rangetable
void zipfinit(char *urlFile, double alpha)
{
    int i;
    char** allUrls = filetoarray(urlFile);
    //struct urlprobpair ** probabilities = calc_probs(allUrls, 1, totalUrls);
    free(allUrls);
    ranges = createurlrangepair2(allUrls, alpha, totalUrls); //alpha equals 1
}


//Taken from ....
//Generate Zipf distributed random variables
//n decides 1 ... N
int zipf(double alpha, int n, int k)
{
//	printf("In function zipf \n");
    static int first = 1;	//Static first time flag
    static double c = 0;	//Normalization constant
    double z;	//Uniform random number (0 < z < 1)
    double sum_prob;	//Sum of probabilities
    double zipf_value;	//Computed exponential value to be returned
    int i; 	//Loop counter

    //Compute normalization constant
    if(first == 1) {
        for(i = 1; i <= n; i++) {
            c = c + (1.0 / pow((double)i, alpha));
            c = 1.0 / c;
            first = 0;
        }
    }

    //printf("Normalized constant c = %f \n", c);
    //Pull a uniform random number (0 < z < 1)
    do {
        printf("k = %d \n", k);
        z = rand_val(k);
//		z = ((double)rand()) / RAND_MAX;
        printf("z = %f \n", z);
    } while((z == 0) || (z == 1));

    // Map z to the value
    sum_prob = 0;

    for(i = 1; i <= n; i ++) {
        sum_prob = sum_prob + c / pow((double) i, alpha);

        if(sum_prob >= z) {
            zipf_value = i;
            break;
        }
    }

    // Assert that zipf_value is between 1 and N
    assert((zipf_value >= 1) && (zipf_value <= n));
    return(zipf_value);
}

//return index of a zipf distribution random name 
int zipfrand(int lineCount)
{
    int randint = rand() % ranges[lineCount - 1] -> range[1];
    int urlIndex = searcharray(randint, ranges, lineCount);
    packetssent[urlIndex] ++;
    return urlIndex;
}




static void
ask_set(struct mydata *md, int flying, int lineCount, char * urlFile, double alpha)
{
    if(DEBUG)
        printf("In function askset \n ");

    struct ccn_charbuf * name = NULL;
    struct ccn_charbuf * templ = NULL;
    int i = 0;
    int res = -1;
    struct ccn_closure * cl = NULL;
    /* TODO: Zipf Distribution
    	//Test the Zipf distribution
    	printf("Zipf starts \n");
    	for(i = 1; i < 100000; i++){
    		printf("%d \n", zipf(5, 100, i));
    	}
    	printf("Zipfs ends\n");
    */
    totalUrls = lineCount;
    zipfinit(urlFile, alpha);
    int r = 0;

    for(r = 0; r < 1; r++) {
        begin = clock();

        for(i = 0; i < flying; i++) {
            cl = &(md->ooo[i].closure);
            name = ccn_charbuf_create();
            char * result;
            int num;

            if (distribution == 1) { //limited uniform
                result = filetoarray(urlFile)[i];
                num = packetssent[i];
                packetssent[i]++;

            } else if (distribution == 2) { //zipf
                int urlIndex = zipfrand(lineCount);
                num = packetssent[urlIndex];
                result = ranges[urlIndex] -> url;

            } else if (distribution == 3) { //uniform
                int urlIndex = rand() % totalUrls;
                num = ranges[urlIndex] -> url;
                result = ranges[urlIndex] -> url;
                packetssent[i]++;
            }

            printf("url = %s \n", result);
            printf("===\n");
            res = ccn_name_from_uri(name, result);

            if(res < 0) {
                printf("ccn_name_from_uri failed \n");
                abort();
            }

            char stringNum[20];
            sprintf(stringNum, "%d", num);
            ccn_name_append(name, stringNum, sizeof(stringNum));
            templ = make_template(md);

            if(DEBUG) {
                //Print out the interest's name
                printf("Sending Interest : ");
                int myres = 0;
                struct ccn_indexbuf* ndx = ccn_indexbuf_create();
                unsigned char* mycomp = NULL;
                size_t mysize = 0;
                ndx->n = 0;
                myres = ccn_name_split(name, ndx);

                if(myres < 0) {
                    fprintf(stderr, "ccn_name_split @ ccntraffic. failed");
                }

                int it = 0;

                for(it = 0; it < ndx->n - 1; it++) {
                    mysize = 0;
                    myres = ccn_name_comp_get(name->buf, ndx, it, &mycomp, &mysize);
                    printf("%s/", mycomp);
                    mycomp = NULL;
                }

                printf("\n");
            }

            res = ccn_express_interest(md->h, name, cl, templ);
            intereststosend = intereststosend - 1;

            if (intereststosend < 0) exit(0);

            if(res < 0) abort();

            ccn_charbuf_destroy(&name);
        }
    }

    printf("Sent!\n");
}


#define CHUNK_SIZE 1024

enum ccn_upcall_res
incoming_content(
    struct ccn_closure *selfp,
    enum ccn_upcall_kind kind,
    struct ccn_upcall_info *info)
{
    struct ccn_charbuf *name = NULL;
    struct ccn_charbuf *templ = NULL;
    struct ccn_charbuf *temp = NULL;
    struct ccn_closure * cl = NULL;
    const unsigned char *ccnb = NULL;
    size_t ccnb_size = 0;
    const unsigned char *data = NULL;
    size_t data_size = 0;
    size_t written;
    const unsigned char *ib = NULL; /* info->interest_ccnb */
    struct ccn_indexbuf *ic = NULL;
    int res;
    struct mydata *md = selfp->data;

    //handler is about to be deregistered
    if (kind == CCN_UPCALL_FINAL) {
        if (md != NULL) {
            selfp->data = NULL;
            free(md);
            md = NULL;
        }

        return(CCN_UPCALL_RESULT_OK);
    }

    if (kind == CCN_UPCALL_INTEREST_TIMED_OUT)
        return(CCN_UPCALL_RESULT_REEXPRESS);

    if (kind != CCN_UPCALL_CONTENT && kind != CCN_UPCALL_CONTENT_UNVERIFIED)
        return(CCN_UPCALL_RESULT_ERR);

    if (md == NULL)
        selfp->data = md = calloc(1, sizeof(*md));

    ccnb = info->content_ccnb;
    ccnb_size = info->pco->offset[CCN_PCO_E];
    ib = info->interest_ccnb;
    ic = info->interest_comps;
    /* XXX - must verify sig, and make sure it is LEAF content */
    res = ccn_content_get_value(ccnb, ccnb_size, info->pco, &data, &data_size);

    if (res < 0) abort();

    if (data_size > CHUNK_SIZE) {
        /* For us this is spam. Give up now. */
        fprintf(stderr, "*** Segment %d found with a data size of %d."
                " This program only works with segments of 1024 bytes."
                " Try ccncatchunks2 instead.\n",
                (int)selfp->intdata, (int)data_size);
        exit(1);
    }

    /* OK, we will accept this block. */
    //sleep(1);
    written = fwrite(data, data_size, 1, stdout);

    if (written != 1)
        exit(1);

    // record received interests
    temp = ccn_charbuf_create();
    ccn_name_init(temp);
    res = ccn_name_append_components(temp, ib, ic->buf[0], ic->buf[ic->n - 2]);
    struct ccn_indexbuf* idx = ccn_indexbuf_create();
    unsigned char* mycomp = NULL;
    size_t  mysize = 0;
    idx->n = 0;
    int myres = 0;
    myres = ccn_name_split(temp, idx);
    myres = ccn_name_comp_get(temp->buf, idx, 0, &mycomp, &mysize);
    char * receivedurl = mycomp;
    int receivurlidx = findurlIndex(receivedurl, ranges, totalUrls);
    //printf("incoming interest name: %s", receivedurl);
    //printf("this url is at index %d", receivurlidx);
    packetsreceived[receivurlidx]++;

    /* A short block signals EOF for us. */
    if (data_size < CHUNK_SIZE)
        exit(0);

    /* Exit program if we have sent all of the required interests. */
    if (intereststosend <= 0) {
        FILE *fp;
        int k;
        end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        fp = fopen("results.txt", "w+");
        fprintf(fp, "Sending and receiving the following packets took %d seconds of CPU time.\n", time_spent);

        for (k = 0; k < totalUrls; k++) {
            fprintf(fp, "%d;%d-%d \n", k, packetssent[k], packetsreceived[k]);
        }

        fflush(fp);
        fclose(fp);
        printf("Sent all requested packets.");
        exit(0);
    }

    intereststosend = intereststosend - 1;
    /* Ask for the next one */
    int i = 2;
    cl = &(md -> ooo[i].closure);
    name = ccn_charbuf_create();
    ccn_name_init(name); //reset charbuf to represent an empty name in binary format

    if(distribution == 1) { //limited uniform
        packetssent[receivurlidx]++;

        if (ic->n < 2) abort();

        res = ccn_name_append_components(name, ib, ic->buf[0], ic->buf[ic->n - 2]);

        if (res < 0) abort();

        temp = ccn_charbuf_create();
        ccn_charbuf_putf(temp, "%d", packetssent[receivurlidx]);
        ccn_name_append(name, temp->buf, temp->length);
        ccn_charbuf_destroy(&temp);

    } else if (distribution == 2) { //zipf
        int urlIndex = zipfrand(totalUrls);
        char * result = ranges[urlIndex]-> url;
        res = ccn_name_from_uri(name, result);

        if (res < 0) {
            printf("ccn_name_from_uri failed \n");
            abort();
        }

        temp = ccn_charbuf_create();
        //printf("intdata = %d \n ", selfp->intdata);
        ccn_charbuf_putf(temp, "%d", packetssent[urlIndex]); // we need to append the count here....
        ccn_name_append(name, temp->buf, temp->length); //add a component to a name, compoenent is arbitrary string of n octects
        ccn_charbuf_destroy(&temp);

    } else if (distribution == 3) { //uniform
        int urlIndex = rand() % totalUrls;
        packetssent[urlIndex]++;
        char * result = ranges[urlIndex] -> url;
        res = ccn_name_from_uri(name, result);

        if (res < 0) {
            printf("ccn_name_from_uri failed \n");
            abort();
        }

        temp = ccn_charbuf_create();
        ccn_charbuf_putf(temp, "%d", packetssent[urlIndex]);
        ccn_name_append(name, temp-> buf, temp -> length);
        ccn_charbuf_destroy(&temp);
    }

    if(DEBUG) {
        //Print out the interest's name
        printf("We have %d interests left to send\n", intereststosend);
        int myres = 0;
        struct ccn_indexbuf* ndx = ccn_indexbuf_create();
        unsigned char* mycomp = NULL;
        size_t mysize = 0;
        ndx->n = 0;
        myres = ccn_name_split(name, ndx);

        if(myres < 0) {
            fprintf(stderr, "ccn_name_split @ ccntraffic. failed");
        }

        int it = 0;
        printf("Interest name =");

        for(it = 0; it < ndx->n - 1; it++) {
            mysize = 0;
            myres = ccn_name_comp_get(name->buf, ndx, it, &mycomp, &mysize);
            printf("%s/", mycomp);
            mycomp = NULL;
        }

        printf("\n");
    }

    templ = make_template(md);
    res = ccn_express_interest(info -> h, name, selfp, templ);

    if (res < 0) abort();

    ccn_charbuf_destroy(&templ);
    ccn_charbuf_destroy(&name);
    return(CCN_UPCALL_RESULT_OK);
}


int main(int argc, char** argv)
{
    struct ccn * ccn = NULL;
    struct ccn_charbuf * name = NULL;
    struct ccn_closure * incoming = NULL;
    const char * arg = NULL;
    int res;
    int micros;
    char ch;
    struct mydata * mydata = NULL;
    int allow_stale = 0;
    int use_decimal = 1;
    int lineMarker = -1;
    int start = 0;
    int flying = -1;
    double alpha = 1;
    int i = 0;
    char* urlFile = NULL;

    //By default, we will send all the urls listed in the file starting from position 0
    while ((ch = getopt(argc, argv, "n:s:f:i:z:a:hd")) != -1) {
        switch (ch) {
        case 'f':
            urlFile = optarg;
            break;

        case 's':
            start = atoi(optarg);
            break;

        case 'i':
            intereststosend = atoi(optarg);
            break;

        case 'z':
            distribution = atoi(optarg);
            break;

        case 'a':
            alpha = atof(optarg);

        case 'n':
            res = atoi(optarg);

            if (1 <= res)
                flying = res;

            else
                usage(argv[0]);

            //printf("flying = %d \n", flying);
            break;

        case 'd':
            DEBUG = 1;
            break;

        case 'h':
        default:
            usage(argv[0]);
        }
    }

    int lineCount = getLineCount(urlFile);

    if(flying == -1)
        flying = lineCount; //By default, all the URLs in the urlFile will be queried

    char line[2000];
    int idx = 0;
    int lineIdx = 0;
    ccn = ccn_create();

    if (ccn_connect(ccn, NULL) == -1) {
        perror("Could not connect to ccnd");
        exit(1);
    }

    mydata = calloc(1, sizeof(*mydata));
    mydata->h = ccn;
    mydata->allow_stale = allow_stale;
    mydata->use_decimal = use_decimal;
    mydata->excl = NULL;
    //Initialize the ooo data
    mydata->ooo = malloc(sizeof(struct ooodata) * flying);

    for (i = 0; i < flying; i++) {
        incoming = &mydata->ooo[i].closure;
        incoming->p = &incoming_content;
        incoming->data = mydata;
        incoming->intdata = -1;
    }

    //Initialize arrays to count incoming and outgoing packets
    packetssent = malloc(lineCount * sizeof(int));
    packetsreceived = malloc(lineCount * sizeof(int));
    //Build and send out a group of interests
    ask_set(mydata, flying, lineCount, urlFile, alpha);
    /* Run a little while to see if there is anything there*/
    res = ccn_run(ccn, 500);
    printf("%d", res);

    /* We got something, run until end of data or somebody kills us */
    while (res >= 0) {
        micros = 50000000;
        printf("%d", res);
        res = ccn_run(ccn, micros / 1000);
    }

    printf("Exiting....");
    fclose(urlFile);
    //Release the dynamically allocated memory
    free(mydata->ooo);
    free(mydata);
    ccn_destroy(&ccn);
    exit(res < 0);
}

