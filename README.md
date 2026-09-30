# IN4230 MiP in C

## Design

### Differences between IPv4 and MIP

First off is the fact that MIP has been implemented with the advantage of hindsight being 20/20. MIP can lower traffic and overhead, but loses to IPv4's ability to be sendt far and wide via different routes and technologies like fragmenting and flow-control. This creates a catch-22, as networks (famously) tend to expand and not shrink.

This means that there could be an argument for smaller networks to implement MIP in their local network, especially if the messages are small, like IoT sensors or devices on a limited data-plan. However, any off-the-shelf device uses IPv4 and implementing your own protocol and system would probably cost more than upgrading the data-plan.  

### The ARP question

IPv4 also doesnt use ARP in the same way as MIP in this case. MIP integrates the ARP mechanism into itself, while IPv4 uses the literal ARProtocol, and is not necessarily connected to every IP transmission. This raises a few questions about the "ethics" of MIP, as we want to keep the network dumb and only process what is needed. In this case, the router / node always checks the message to see if it is an ARP request - which also implies it can read information that it is not privvy to.

### Flowchart
```
                     +---------------------------+
                     |         START             |
                     +---------------------------+
                                   |
                                   v
                     +---------------------------+
                     |    1. Initialization      |
                     |  - Parse MIP Address      |
                     |  - Open AF_PACKET Socket  |
                     |  - Create IPC Socket      |
                     |  - Init ARP Cache         |
                     +---------------------------+
                                   |
                                   v
                     +---------------------------+
                     |   2. Event Loop Wait      |
                     |  (select / epoll / poll)  |
                     +---------------------------+
                                   |
         +-------------------------+-------------------------+
         |                                                   |
         v                                                   v
[ Activity on IPC Socket ]                       [ Activity on Raw Socket ]
         |                                                   |
         v                                                   v
+------------------------+                       +------------------------+
| Read Application Frame |                       | Read Ethernet Frame    |
| (Dest MIP + SDU Data)  |                       | validate Eth Type      |
+------------------------+                       | (0x88B5)               |
         |                                       +------------------------+
         v                                                   |
+------------------------+                                   v
| Lookup Dest MAC in     |                       +------------------------+
| ARP Cache              |                       | Extract MIP Header     |
+------------------------+                       | Parse TTL, SDU Len,    |
         |                                       | & SDU Type             |
   +-----+-----+                                 +------------------------+
   |           |                                             |
[ Found ]  [ Not Found ]                                     v
   |           |                                 +------------------------+
   |           v                                 | Auto-Learn / Update    |
   |   +-------------------+                     | Src MAC/MIP in ARP     |
   |   | Send MIP-ARP Req  |                     | Cache                  |
   |   | (Broadcast, TTL=1)|                     +------------------------+
   |   +-------------------+                                 |
   |           |                                             v
   |           v                                   /-------------------\
   |   [ Queue App Data ]                         <  Inspect SDU Type   >
   |           |                                   \-------------------/
   |           |                                     /               \
   |           |                       [ SDU = 0x01 (ARP) ]     [ SDU = 0x02 (Ping) ]
   |           |                                |                         |
   v           v                                v                         v
+------------------------+            +-------------------+    +-------------------+
| Build MIP Header       |            | Is ARP Request    |    | Is Dest = Local   |
| - Dest / Src MIP       |            | for Local Address?|    | or Broadcast?     |
| - TTL (1 or 64)        |            +-------------------+    +-------------------+
| - SDU Len & SDU Type   |                      |                        |
+------------------------+               +------+------+          +------+------+
         |                               |             |          |             |
         v                            [ Yes ]        [ No ]    [ Yes ]        [ No ]
+------------------------+               |             |          |             |
| Transmit over          |               v             v          v             v
| AF_PACKET Socket       |      +---------------+  [ Ignore ] +-------+    [ Drop / ]
+------------------------+      | Send MIP-ARP  |             | Forward |  [ Forward]
         |                      | Response      |             | Payload |
         |                      +---------------+             | to IPC  |
         |                              |                     +-------+
         +------------------------------+-------------------------+
                                        |
                                        v
                          +---------------------------+
                          |   Return to Event Loop    |
                          +---------------------------+
```

**NB: Stylized and formatted by Gemini**

### AI Disclaimer

AI has been utilized a substantial amount in the coding process (whether I'd liked to or not (thanks Google search)). Seeing as I come from another university where C was not really taught.
Learning C and sockets at the same time was quite a challenge, so i have used AI for a lot of skeleton-code and function definition.
I am genuinely interested in learning C so I was always trying to implement first, and understand after. I prefer learning by jumping into the deep end and working the pieces out bit by bit, and is how I did most of the assignment. (They are also quite dumb and get incredible scope-creep).

All my words here are my own - as I do not use AI for writing. Potential mishaps in the implementation are my fault.
