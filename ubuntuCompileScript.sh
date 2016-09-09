chmod 777 findversion.sh
chmod 777 configure
sudo apt-get install libsdl2-dev
sudo apt-get install libsdl-dev
sudo apt-get install liblzma-dev
sudo apt-get install liblzo2-dev
./configure
make &> logOfCompileOnUbuntu.txt