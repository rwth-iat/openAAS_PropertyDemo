# openAAS_PropertyDemo
This repository delivers a small demonstrator for the property model and the message transmission with opc ua. The implementation is based on [open62541](http://open62541.org/).

The project can be compiled for a raspberry pi (Version B+). It implements a basic demo for a combined temperature and humidity sensor.  

# Building the demo on raspberry pi

Clone the repository recursively to get all dependencies

```bash
git clone --recursive https://github.com/acplt/openAAS_PropertyDemo 
```
navigate to the new directory
```
cd openAAS_PropertyDemo
```
Generate Makefiles with cmake

```
cmake .
```

Compile
```
make all
```
