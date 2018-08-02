# Yggdrasil
Yggdrasil Public Repository (Version 2)

This project contains the code of Yggdrasil, a framework tool to develop distributed protocols and applications on wireless adhoc networks in linux based systems.

Inside there are two directories containing source code:

* Yggdrasil-LowLevelLib : This contains code related to configuring a device using low level operations.

* Yggdrasil: This contains code to develop protocols and applications and some examples.

### Compiling

In order to compile Yggdrasil you need to have the following depencies installed:

* libnl3
* uuid
* cmake

Cmake will generate a makefile to compile Yggdrasil and its applications.
To do so use the following command:

1. cmake .
2. make

Inside Yggdrasil's project you can find examples on how to add and customize the compilation.
These are present in the files CMakeList
