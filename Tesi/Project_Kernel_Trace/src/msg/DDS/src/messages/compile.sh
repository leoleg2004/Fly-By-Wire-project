#!/bin/sh

# 	Run it by typing 
#	. compile.sh

rm -r src
mkdir src
cp idl/*.idl src/

do_make () {
if [ -z $1 ]
# Checks if any params.
then
   echo "No parameters passed to function."
   return 0
else
   echo "Making $1."
   DDS_MSG=$1
   export DDS_MSG

   cd src
   fastddsgen -I ./ $1.idl
#   fastddsgen -I ../idl/stresa ../idl/$1.idl
   make -f ../idl/makefile
   cd ..
fi
}

do_make std_msgs
do_make geometry_msgs
do_make nav_msgs
do_make rover_msgs
do_make sensor_msgs
#do_make aurora_msgs
#do_make aruco_msgs
#do_make map2d_msgs

rm -r src

