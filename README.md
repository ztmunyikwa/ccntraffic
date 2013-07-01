ccntraffic (a ccnx traffic generator)

Q: How to install ccntraffic and ccndelphi?
A: Place ccntraffic.c and ccndelphi.c under ccnx/csrc/cmd directory, and modify dir.mk to include these two files when building ccnx.

Q: How to run ccntraffic
A: Please follow the tutoiral at http://wiki.arl.wustl.edu/onl/index.php/CCNx:_traffic_generation


Q: How to run zipf.c
As of right now, you must enter three arguments when running zipf.c
./zipf {filename} {alpha} {sample size}

where filename is the file with all of the potential urls, alpha corresponds with the shape of the zipf distribution you want and sample size is the number of randomly generated urls you want.
the results will be placed in a file entitiled results.txt in your working directory (a list of all the randomly generated urls, along with statistics about the run you performed). 
