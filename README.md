# openAAS_PropertyDemo
This repository delivers a small demonstrator for the property model and the message transmission with opc ua. The implementation is based on [open62541](http://open62541.org/).

The project can be compiled for a raspberry pi (Version B+). It implements a basic demo for a combined temperature and humidity sensor ("intelligent field device"). The demo was tested with Raspbian and Ubuntu (runs without measured values). It provides an OPC UA Server with three Endpoints. There is an engineering endpoint which let's you browse and view the whole Address-space. Within the Address-Space there are 4 asset administration shells that contain specific information about the asset in form of property value statements.
There is another endpoint called "Message" which limits the view on a certain set of nodes related to the local message system representative (LMSR). This endpoint delivers the functionality of a "message"-based interaction with the administration shell. Therefore, it provides a method node "dropMessage" with two arguments, one to specify the receiving administration shell and one for the message. 
The third endpoint is a read-only endpoint. That means the connected client shall not be allowed to interact with the administration over this endpoint.

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
# About the demo

<img src="https://github.com/acplt/openAAS_PropertyDemo/blob/master/img/openAAS_PropertyDemo_arch.png" width="384">




The OPC UA demonstrator shows 4 Asset Administration Shells (AAS) that provide basic information of the shells identities and their contained assets. At the moment, the property information is borne by the corresponding "property value statement". All AAS share some certain common properties: 
* Every AAS has a unique identifier that identifies the AAS. 
* Every AAS has a unique identifier that identifies the comprised asset 

Within the OPC UA server, the property value statements are structured in lists. Each list is related to only one property carrier, that is in this case the AAS or the asset.
