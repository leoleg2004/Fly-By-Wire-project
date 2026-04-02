#!/bin/sh

# Run it by typing 
#	. compile.sh

rm -r src
mkdir src
cp *.idl src/

do_make () {
if [ -z $1 ]
# Checks if any params.
then
   echo "No parameters passed to function."
   return 0
else
   echo "Making $1."
   ERTS_MSG=$1
   export ERTS_MSG

   cd src
   ../../fastddsgen_tool/scripts/fastddsgen -I ./ $1.idl

   echo "Patching .h -> .hpp per compatibilità FastDDS 3..."
   python3 ../patch_fastdds3.py .

   make -f ../makefile
   cd ..
fi
}

do_make geometry_msgs
do_make student_msgs
do_make service_msgs
do_make thread_msgs


rm -r src

