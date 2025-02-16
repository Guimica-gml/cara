#! /bin/sh

SRC=$1
BUILD=$2
INCL=$3
BASE=$4
BRANCH=$5
CMP=$6
PRINT=$7
NAME=$8
FILE=$9

cp $SRC/btree/stencil.h $BUILD/$FILE.h
cp $SRC/btree/stencil.c $BUILD/$FILE.c
sed -e "s/@basetype@/$BASE/g" -i $BUILD/$FILE.h
sed -e "s/@branching@/$BRANCH/g" -i $BUILD/$FILE.h
sed -e "s/@cmp@/$CMP/g" -i $BUILD/$FILE.h
sed -e "s/@print@/$PRINT/g" -i $BUILD/$FILE.h
sed -e "s/@treename@/$NAME/g" -i $BUILD/$FILE.h
sed -e "s/@filename@/$FILE/g" -i $BUILD/$FILE.h
sed -e "s/@include@/$INCL/g" -i $BUILD/$FILE.h
sed -e "s/@basetype@/$BASE/g" -i $BUILD/$FILE.c
sed -e "s/@branching@/$BRANCH/g" -i $BUILD/$FILE.c
sed -e "s/@cmp@/$CMP/g" -i $BUILD/$FILE.c
sed -e "s/@print@/$PRINT/g" -i $BUILD/$FILE.c
sed -e "s/@treename@/$NAME/g" -i $BUILD/$FILE.c
sed -e "s/@filename@/$FILE/g" -i $BUILD/$FILE.c
sed -e "s/@include@/$INCL/g" -i $BUILD/$FILE.c
