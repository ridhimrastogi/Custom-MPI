#!/bin/bash

#Usage: ./my_prun [NPROC] [CMD]

[ $# -ne 2 ] && { echo "Usage: $0 [NPROC] [cmd]"; exit 1; }

# Set some variables
NUMPROC=$1
CMD=$2
NODEFILE="nodefile.txt"
PWD=$(pwd)

# Parse $SLURM_NODELIST into an iterable list of node names
echo $SLURM_NODELIST | tr -d c | tr -d [ | tr -d ] | perl -pe 's/(\d+)-(\d+)/join(",",$1..$2)/eg' | awk 'BEGIN { RS=","} { print "c"$1 }' > $NODEFILE

# For each item in the nodefile, connect via ssh and run the cmd.
# The -n parameter is important or ssh will consume the rest 
# of the loop list in stdin.
# Increment rank passed to the code for each node

rank=0
for curNode in `cat $NODEFILE`; do
  ssh -o StrictHostKeyChecking=no -n $curNode "cd $PWD;$CMD $curNode $rank $NUMPROC $NODEFILE" & pid[$rank]=$!
  (( rank++ ))
done

#wait for each ssh / corresponding CMD to finish executing before exiting
rank=0
for curNode in `cat $NODEFILE`; do
  wait ${pid[$rank]}
  (( rank++ ))
done

#rm $NODEFILE
