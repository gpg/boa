#! /bin/sh
DT=`date --date 'now' +%s`
(cd ..; tar cvfz boa-$DT.tar.gz src --exclude src/CVS --exclude src/*.o --exclude /src/*~)
