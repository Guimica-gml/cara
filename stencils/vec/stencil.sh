#! /bin/sh

SRC=$1
BUILD=$2
INCL=$3
BASE=$4
NAME=$5
FILE=$6

cp $SRC/vec/stencil.h $BUILD/$FILE.h
cp $SRC/vec/stencil.c $BUILD/$FILE.c
sed -e "s/@basetype@/$BASE/g" -i $BUILD/$FILE.h
sed -e "s/@vecname@/$NAME/g" -i $BUILD/$FILE.h
sed -e "s/@filename@/$FILE/g" -i $BUILD/$FILE.h
sed -e "s/@include@/$INCL/g" -i $BUILD/$FILE.h
sed -e "s/@basetype@/$BASE/g" -i $BUILD/$FILE.c
sed -e "s/@vecname@/$NAME/g" -i $BUILD/$FILE.c
sed -e "s/@filename@/$FILE/g" -i $BUILD/$FILE.c
sed -e "s/@include@/$INCL/g" -i $BUILD/$FILE.c
