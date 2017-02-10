#!/bin/sh

echo '----------------------------------'
echo 'install python package RPIO2'
echo '                                  '

cd /home/pi/scratchClient/RPIO2
rm -f -R RPIO2

tar zxf RPIO2.tar.gz
cd RPIO2

sudo python setup.py install

cd ..
echo 'complete'
echo '----------------------------------'
echo ' '
