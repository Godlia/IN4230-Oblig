# Task

The main focus of the task is to build and test a very simple network stack. We will use a protocol called MIP, which stands for “Minimal Interconnection Protocol”. MIP is a minimal network layer protocol, created for IN3230/IN4230, that has only the bare necessities in the header and avoids overhead, making it quick to process and minimising latency.

The programs you write will run on a virtual network managed by mininet inside a virtual machine. You will have to write three programs:

1. a MIP daemon
2. a Ping server
3. a Ping client

These are described in more details below.

Finally, it is important to document your work, so you will be writing a brief design document. You will also need to analyze some design choices we have made.

## MIP daemon

The MIP daemon implements the network layer in our stack. A single instance of the daemon is permanently active on every host participating in the network.

### MIP daemon components

The MIP daemon is comprised of several components that implement different aspects required to provide the MIP network layer's services:

* The interface with the layer above. Conventionally this would be the transport layer, but for the time being we will connect applications directly above the MIP layer. This component is responsible for encapsulating the payload from the upper layer as a Service Data Unit (SDU) in a MIP datagram and populating the MIP header fields. The entirety of the MIP datagram, that is the headers combined with the SDU, is referred to as the MIP Protocol Data Unit (PDU).
* The interface with the layer below, in this case the link layer. We will only concern ourselves with interfacing with an Ethernet link layer. This component must take care of framing the MIP PDU as an Ethernet frame.
* A MIP to physical address mapping service, the MIP Address Resolution Protocol (MIP-ARP). Conceptually this is a protocol which resides both directly above the network layer (being an application of sorts) as well as on the interface between MIP and the link layer (because it needs access to Ethernet address). Because of this somewhat odd layering placement and as it is so tightly integrated with the network layer we will implement it directly in the MIP daemon.

Combined, these three components will enable the MIP daemon to provide a basic point-to-point networking service. In other words, directly connected neighbours can communicate with each other, but for now we will not provide any way to communicate across intermediate nodes (no multi-hop communication).

### MIP-ARP

To be able to communicate across Ethernet, hosts need to know each other's Ethernet link layer addresses (MAC addresses).

It would be highly impractical to force users to know these lower-layer details, therefore we need a way of automatically learning the mapping between MIP and MAC addresses. The MIP-ARP protocol provides this service and is specified in detail in the MIP specification linked below.

These mappings are required for every outgoing transmission, so it would be rather inefficient to repeat such lookups for hosts we have looked up previously. Solve this by implementing a MIP-ARP cache table that stores recent lookup results.

### Implementation details

We have specified the MIP and MIP-ARP protocols in an RFC-like document which you can find here.

The MIP daemon will listen on two sockets:

* a RAW socket capturing Ethernet frames with a given Ethertype identifying MIP traffic: ETH_P_MIP to be defined as 0x88B5. The same socket is also used to transmit outbound traffic.
* a UNIX domain socket which functions as the interface with the upper layers.

The protocol for communicating with upper layers is as follows:

* Whenever there is an SDU to be passed either up or down, this is sent as a message over the UNIX socket.
* The message format consists of: An 8 bit MIP address, which is either the sender or destination address when passing up/down, respectively. The rest of the message contains the payload (SDU).
* In this assignment, it is sufficient to use blocking socket calls when dealing with this interface socket. We do not support multiple upper layer processes for the time being.

The user must configure the MIP daemon with an address uniquely identifying the current host on the MIP network.

Users must also provide a filesystem path indicating where the daemon should bind a UNIX domain socket, which will be used as the interface to the upper layer(s).

If the user optionally passes a debug-mode flag on the command line, every communication should be logged to the console with at least the following status information:

* Source and destination Ethernet addresses
* Source and destination MIP addresses
* Current content of the MIP-ARP cache

#### **Command line argument specification**

    mipd [-h] [-d] <socket_upper> <MIP address>

Options:

* **-d**: enable debugging
* **-h**: prints help and exits

Arguments:

* **socket_upper**: pathname of the UNIX socket used to interface with upper layers.
* **MIP address**: the MIP address to assign to this host

## Ping server

The ping server conceptually ought to reside in the application layer, but we will interface it directly with the network layer for now.

This server will receive text messages via IPC from the local MIP daemon. It should print this text and send back a ping message with content “PONG:\<received message>”.

### Command line argument specification

Command line argument specification

    ping_server [-h] <socket_lower>

Options:

* **-h**: prints help and exits the program

Arguments:

* **socket_lower**: pathname of the socket that the MIP daemon uses to communicate with upper layers (MIP is a lower layer from the point of view of the ping application).

## Ping client

Akin to the ping server, the ping client is an application, but again we will attach it directly above the network layer.

The ping client sends ping messages through the network layer. These message contain user-specified content and are addressed to some user-specified MIP address. Ping messages are built up in the following manner: "PING:\<user-specified message>".

Upon receiving a response, it prints the time between sending the message and obtaining the response. Outgoing requests can be correlated with their corresponding responses by matching the message echoed by the server in the "PONG" reply. Unknown messages can be ignored.

The ping client sends a ping, and after receiving a response and printing the result it exits. If no response arrives within 1 second, the ping client should print “timeout” and exit.

### Command line argument specification

    ping_client [-h] <socket_lower> <message> <destination_host>

Options:

* **-h**: prints help and exits

Arguments:

* **socket_lower**: pathname of the socket that the MIP daemon uses to communicate with upper layers (MIP is a lower layer from the point of view of the ping application).
* **message**: the message that needs to be sent
* **destination_host**: MIP address of the destination host

**NOTE**: Make sure to keep the same exact application names (mipd, ping_client, ping_server) and the same exact order of the command line arguments specified above, since the examiners use a script to correct the assignments.

## Example scenario

https://www.uio.no/studier/emner/matnat/ifi/IN3230/h26/obligatorisk-oppgave/example-scenario.html

## Development and test environment

You may develop and test your implementation on a virtual machine image that we provide. The virtual machine will be hosted on the NREC cloud infrastructure. For detailed information and instructions on using the virtual machine platform, see here.

You are allowed to develop the assignment on your own machine, but your submitted implementation MUST run on the provided virtual machine. Sensors will test only on the VM and will not install additional libraries or software. Submissions that cannot be compiled or executed on the VM, or that rely on extra packages, tools, or environment changes not available there, will be automatically failed.

## Submission details
You must submit the following:

1. A design document that contains:
    * A discussion of how MIP is different than IPv4, and how its performance compares to IPv4 (one to two paragraphs).
    * Discuss why we need to handle the MIP-ARP protocol as a special case. Discuss alternative design choices that would allow for a cleaner implementation (two to three paragraphs).
    * A flow chart summarizing the flow of execution on both the client and server side.
    * A short disclaimer indicating how you utilized AI for this assignment.

2. Program code, where the code is well commented. Document all the variables and definitions. For each function in the program, you must document the following:
    * What the function does.
    * What input and output parameters mean and how they are used.
    * Which global variables affect the function.
    * What the function returns.
    * Other characteristics that are important to know about (eg. error situations).

Example comment:

```c
/**
 * Convert string to all-uppercase
 * str: pointer to input string.
 * dest: pointer to output buffer.
 * The buffer pointed at by dest must be allocated
 * and large enough to hold all of str.
 *
 * Returns the number of characters changed.
 */
int to_upper(char *str, char **dest)
{ ...
```

## Submission requirements

The design document edited using an appropriate tool, eg. LaTeX, Word, etc. The document should contain the items listed above.

Before delivery, the document shall be converted to PDF format. Neither a Word / Works / Open Office / TeX - document nor a regular editor file (plain text) will be accepted.

The code must consist of compilable C source files. We highly recommend including a Makefile for easy compilation.

Your solution must compile and run on the provided virtual machine image, without requiring additional configuration.

The scope of the design document need not necessarily be large, but must contain sufficient information to meet the requirements described under ‘Submission materials’. The important thing is to document understanding of the relevant topics, in addition to the actual implementation.

### Electronic submission

All materials must be submitted electronically where all files (Makefile, readme.pdf, *.c files, and any possible subdirectories for headers or utility functions) are collected in one main directory using your UiO username as a filename. Create one compressed tar-ball containing that directory. Use the command:

    tar -zcv --owner=nobody --group=nobody -f username.tgz username

Deliver your submission through the Devilry system. You may deliver as many times as you like before the deadline, we will only consider the latest submission.
