# Configuring lwIP for compatibility with sACN                                  {#configuring_lwip}

Lightweight IP (lwIP) is a lightweight embedded TCP/IP stack. When including sACN in a lwIP
project, the following lwIP configuration options are required:

* **Required options**
  + `LWIP_IPV4=1`: sACN devices are required to support IPv4.
  + `NO_SYS=0`: EtcPal uses the lwIP sockets API which requires this.
  + `LWIP_SOCKET=1`: EtcPal uses the lwIP sockets API which requires this.
  + `SO_REUSE=1`: Allows sACN multicast reception to work properly.
  + `SO_REUSE_RXTOALL=1`: Allows sACN multicast reception to work properly.
  + `LWIP_IGMP=1`: Allows sACN to play nicely with multicast network infrastructure.
