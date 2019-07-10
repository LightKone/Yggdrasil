# File Dissemination Application

Here you can find a file dissemination application built using the abstractions provided by the Yggdrasil Framework and Runtime system.

The application is composed by a File Dissemination Server (found in ``main.c``) and a Client (found in ``tools/transfer.c``).

## Compiling and Executing

The following assumes that a ``build/``, ``bin/``, and ``lib/`` folders exist in your **project directory**.

- `build/` - will contain compilation rules.
- `bin/` - will contain binary executables.
- `lib/` - will contain static library. 

### Instructions for compiling

1. `cd build` and `cmake ..` - this will create compilation rules for the whole Yggdrasil Project in `build/`
2. (still in `build/`) `make yggFileTransfer` - the Yggdrasil project contains multiple targets, which are not relevant for this application. The target `yggFileTransfer` will build both the File Dissemination Server and the Client.

This will create the following binaries in `bin/`:
1. `yggFileServer` - The File Dissemination Server 
2. `yggTransfer`   - The Client

### Instructions for executing

#### File Dissemination Server

`yggFileServer [-d <working directory>] <nodeAddress> <nodePort> [<contactAddress> <contactPort>]`

The File Dissemination Server `yggFileServer` contains two mandatory arguments:
   1. `<nodeAddress>` - the IP address of the local process.
   2. `<nodePort>` - the port of the local process.
   
The applications uses the arguments `<contactAddress> <contactPort>` to connect to a process which is already in the system. If omitted, the process will assume that it is the first process of the system.

The optional argument `[-d <working directory>]` serves to specify the directory to where the process will read and write disseminated files to.

By the default `<working directory>` is set to `~/files/<nodePort>`, which will be created if they don't exist. 

#### Client

`yggTransfer [-s <serverAddress>] <path/to/file>`

The Client will transfer the local file `<path/to/file>` to the process identified by the address `<serverAddress>`, which by default is set to `127.0.0.1`.

Upon a successful first transfer, the File Dissemination Server will begin to disseminate the file to the remainder of the system.

## Server Architecture

The File Dissemination Server is composed by 3 protocols found in Yggdrasil, an alternative Dispatcher protocol, and a simple command interpreter.

### Protocols
* HyParView - manages the membership of the system
* PlumTree - disseminates information throughout the system
* File Reader & Writer - reads & writes the disseminated files to disk

In particular the Server uses the following protocols found in the Yggdrasil Project:
* `/protocols/ip/membership/hyparview.c` - a faithful implementation of the HyParView protocol.
* `/protocols/ip/dissemination/plumtree_mem_optimized_flow_control.c` - a version of the PlumTree protocol that is enriched with a flow control mechanism and that refrains from storing in-memory the contents of messages.
* `/protocols/ip/data_transfer/block_data_transfer_mem_opt.c` - a protocol that reads and writes files in blocks (of a page size `~4Kb`).

More details on these protocols can be found in [here](../../protocols/ip). 

### Dispatcher
Due to the size messages, the application leverages an alternative Yggdrasil Dispatcher Protocol that performs asynchronous sends/receives for large messages.

The code for this Dispatcher can be found in the Yggdrasil Project at `/protocols/ip/dispatcher/multi_tcp_socket_dispatcher.c`

### Application Layer
The application layer contains a dedicated thread to receive incoming requests from the client. The client will transfer a file, which will be written into the working directory, and then requested to disseminate by the File Reader & Writer Protocol.

The Client will get reply from the File Dissemination Server, stating that the dissemination process has begun or that an error occurred in the Server side.


## Missing Features

The application should be pretty stable. However there are some missing feature that should be incorporated:

- [ ] Recuperating File Parts. - The only mechanism implemented to recuperate "lost" file parts is in the natural operation of the dissemination protocol. If a process joins the system in the middle of a file transfer it will not ask for the missing parts.

 
- [ ] Add a Staging area for files whose transmission is not yet complete.
