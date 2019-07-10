# Yggdrasil IP Networks Test Applications

Each `.c` file in this folder is mapped to a build target.

Here, we will guide through the existing targets, on how to compile them, execute them, as well as their dependencies.


## Existing targets

| Target | File |
| ----------- | ----------- |
| `test_hyparview` | `hyparview_test.c` |
| `test_plumtree` | `plumtree_test.c` |
| `test_xbot` | `xbot_test.c` |
| `test_tls` | `tls_test.c` |
| `test_multi_tcp_dispatcher` | `multi_dispatcher_test.c` |
| `file_server` | `plumtree_file_transfer.c` |
| `simple_transfer_server` | `simple_transfer_test.c` |
| `block_transfer_server` | `block_transfer_test.c` |
| `block_transfer_opt_server` | `block_transfer_mem_opt_test.c` |
| `block_transfer_opt_flow_server` | `block_transfer_mem_flow_opt_test.c` |
| `block_transfer` | `block_transfer_full_test.c` |
| `ip_tests` | builds all the above targets |

To compile the targets you first need to build the compilation rules by issuing `cmake <project dir>`. This assumes that a `build/`, `lib/` and `bin/` folders in your project directory.

It is advised to issue the command `cmake` from the `build/` directory as to not contaminate your project directory with the compilation rules. As such, the compilation proceeds as follows from your project directory:
1. `cd build`
1. `cmake ..`
1. `make <target name>`

The executable binaries will be placed in the `bin/`.

**Note:** `make` should be called from the same directory where you issued the command `cmake`.

**Note2**: Calling `make` without any arguments will build **all** targets in Yggdrasil.  

You can change the name of the targets in the `CMakeList.txt` in this folder.


***

### Executing the Tests:

We omit the base path for the following binary executables. If you are in your project directory use: `bin/<target name>`; if you are in `bin/` then simply use: `./<target name>`  

#### `test_hyparview`

This application tests the HyParView protocol (defined [here](../../../protocols/ip/membership/hyparview.c)). 

`test_hyparview <node address> <node port> <contact address> <contact port>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).

If `<contact address>` and `<contact port>` are equal to `<node address>` and `<node port>` respectively, the local process will not perform a *join* operation. Use this for the first process of the system. 

***

#### `test_plumtree`

This application tests the PlumTree protocol (defined [here](../../../protocols/ip/dissemination/plumtree.c)) with the HyParView protocol (defined [here](../../../protocols/ip/membership/hyparview.c)) as the membership layer. 

`test_plumtree <node address> <node port> <contact address> <contact port> <transmit> <build tree>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).
* `<transmit>` - boolean stating if the local process should issue dissemination requests (`'0'`: false; or `'1'`: true).
* `<build tree>` - boolean stating if the local process should build the dissemination tree (`'0'`: false; or `'1'`: true).

This application issues periodic dissemination requests if `<transmit>`is set to true. 

The application waits for a longer period of time to begin requesting dissemination messages if `<build tree>` is set to false. 

The first 4 arguments are the same as in `test_hyparview`.

***

#### `test_xbot`

This application tests the X-Bot protocol (defined [here](../../../protocols/ip/membership/xbot.c)).

`test_xbot <node address> <node port> <contact address> <contact port>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).

All arguments are the same as in `test_hyparview`.

***

#### `test_tls`

This application tests an alternative dispatcher ((defined [here](../../../protocols/ip/dispatcher/simple_tls_dispatcher.c))) that enriches the default Yggdrasil dispatcher with TLS sockets.

`test_tls <node address> <node port> <contact address> <contact port>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).

The application sends messages periodically to the process identified by IP address `<contact address>`:`<contact port>`.

***

#### `test_multi_tcp_dispatcher`

This application tests an alternative dispatcher (defined [here](../../../protocols/ip/dispatcher/multi_tcp_socket_dispatcher.c)) that performs asynchronous sends and receives for large messages.

The application is otherwise identical to `test_plumtree`.

`test_multi_tcp_dispatcher <node address> <node port> <contact address> <contact port> <transmit> <build tree>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).

All arguments are the same as in `test_plumtree`.

***

#### `file_server`

This application uses 2 protocols:
* HyParView ([implementation](../../../protocols/ip/membership/hyparview.c)).
* PlumTree ([implementation](../../../protocols/ip/dissemination/plumtree.c)).

And an alternative Dispatcher:
* Multi TCP Socket Dispatcher ([implementation](../../../protocols/ip/dispatcher/multi_tcp_socket_dispatcher.c)).

To disseminate files across the network.

`file_server <node address> <node port> <contact address> <contact port> <serve commands>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).
* `<serve commands>` - boolean stating if the local process should accept commands from a remote client (`'0'`: false; or `'1'`: true).

The first 4 arguments are the same as in `test_hyparview`.

***

#### `simple_transfer_server`

This application delegates writing and reading files to the Simple Transfer Protocol (detailed [here](../../../protocols/ip/data_transfer/block_data_transfer.c)). It is otherwise similar to the previous one.

`simple_transfer_server <node address> <node port> <contact address> <contact port> <serve commands>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).
* `<serve commands>` - boolean stating if the local process should accept commands from a remote client (`'0'`: false; or `'1'`: true).


All arguments are the same as in `file_server`.

***

#### `block_transfer_server`

This application changes the Simple Transfer protocol to the Block Transfer protocol ([implementation](../../../protocols/ip/data_transfer/block_data_transfer.c)).

`block_transfer_server <node address> <node port> <contact address> <contact port> <serve commands>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).
* `<serve commands>` - boolean stating if the local process should accept commands from a remote client (`'0'`: false; or `'1'`: true).

All arguments are the same as in `file_server`.

***

#### `block_transfer_opt_server`

This application builds over the previous one to enrich the protocols used in the file dissemination to be memory optimized.

The application uses:
1. PlumTree with Memory Optimization ([implementation](../../../protocols/ip/dissemination/plumtree_mem_optimized.c)). 
1. Block Transfer with Memory Optimization ([implementation](../../../protocols/ip/data_transfer/block_data_transfer_mem_opt.c)).

`block_transfer_opt_server <node address> <node port> <contact address> <contact port> <serve commands> <working dir>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).
* `<serve commands>` - boolean stating if the local process should accept commands from a remote client (`'0'`: false; or `'1'`: true).
* `<working dir>` - path to directory to where files will be read and writen. This should be an absolute path (*e.g.,* `/home/username/files`).

The first 5 arguments are the same as in `file_server`.

A change was made from the previous application in which, the directory to which files will be written and read needs to be provided in `<working dir>`.

***

#### `block_transfer_opt_flow_server`

This application builds over the previous one to test the PlumTree protocol with Flow Control (detailed [here](../../../protocols/ip/dissemination/plumtree_mem_optimized_flow_control.c)).

`block_transfer_opt_flow_server <node address> <node port> <contact address> <contact port> <serve commands> <working dir>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).
* `<serve commands>` - boolean stating if the local process should accept commands from a remote client (`'0'`: false; or `'1'`: true).
* `<working dir>` - path to directory to where files will be read and writen. This should be an absolute path (*e.g.,* `/home/username/files`).

All arguments are the same as in `block_transfer_opt_server`.

***

#### `block_transfer`

This application is similar to the above `block_transfer*` applications. The application adds the possibility to test different dissemination protocols over the same code base. 

`block_transfer <node address> <node port> <contact address> <contact port> <serve commands> <dissemination protocol> <working dir>`

The arguments are as follows:
* `<node address>` - the IP address of the local process (a string; *e.g.,* `127.0.0.1`).
* `<node port>` - the port of the local process (a numeric: *e.g.,* `9000`).
* `<contact address>` - the IP address of the contact process (a string; *e.g.,* `127.0.0.1`).
* `<contact port>` - the port of the contact process (a numeric: *e.g.,* `9000`).
* `<serve commands>` - boolean stating if the local process should accept commands from a remote client (`'0'`: false; or `'1'`: true).
* `<dissemination protocol>` - An option (`0-3`) that states which dissemination protocol to use. 
* `<working dir>` - path to directory to where files will be read and writen. This should be an absolute path (*e.g.,* `/home/username/files`).


Dissemination protocol options:
* `0` - Flood ([implementation](../../../protocols/ip/dissemination/flood.c)).
* `1` - Flood with Flow Control ([implementation](../../../protocols/ip/dissemination/flood_flow_control.c)).
* `2` - PlumTree with Memory Optimization ([implementation](../../../protocols/ip/dissemination/plumtree_mem_optimized.c)).
* `3` - PlumTree with Memory Optimization & Flow Control ([implementation](../../../protocols/ip/dissemination/plumtree_mem_optimized_flow_control.c)).


The remainder of the arguments are the same as in `block_transfer_opt_server`.
