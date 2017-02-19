# scratchClient
scratchClient is a client software for scratch1.4 remote sensor protocol

It is designed to enable GPIO access on Raspberry Pi and to support SPI or I2C-based devices. Many of the functionality is also available on windows operating system.

See http://heppg.de/ikg/wordpress/?page_id=6 for more information and download.

Quick install guide: Open a terminal and execute the following lines.
```  
cd ~
wget http://heppg.de/ikg/administration/pi/scratchClient/download/scratchClient.tar.gz
tar xzf scratchClient.tar.gz
chmod +r -R scratchClient/
sudo apt-get update
sudo apt-get install python-pip python-dev
sudo pip install cherrypy==8.1.0 ws4py==0.3.5 mako==1.0.6 routes==2.4.1 spidev pyserial intelhex
```

Specialized adapters may need more packages to install. See the docs for more information.