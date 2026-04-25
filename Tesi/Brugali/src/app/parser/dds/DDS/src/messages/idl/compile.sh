#!/bin/sh

# This script init the terminal for compiling STRESA messages
# Run it by typing 
#	. compile.sh


do_make () {
if [ -z $1 ]
# Checks if any params.
then
   echo "No parameters passed to function."
   return 0
else
   echo "Making $1."
   STAR_MSG=$1
   export STAR_MSG


   cp std_msgs.idl src/
   cp geometry_msgs.idl src/
   cp $1.idl src/
   cd src
   fastddsgen  $1.idl 
   make -f ../makefile
   cd ..
   echo ""
fi
}

mkdir src

do_make std_msgs
#do_make geometry_msgs
#do_make nav_msgs
#do_make sensor_msgs

rm -r src/


