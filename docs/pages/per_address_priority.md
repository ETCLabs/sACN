# Per Address Priority                                  {#per_address_priority}
The sACN protocol specification details DMX-style control over TCP/IP networks.  While this provides a fast and efficient mechanism to transport the well-understood DMX protocol, it also introduces a problem that may not be as well-understood by the industry: multiple sources.  Unlike DMX over RS485, multiple sources may easily provide overlapping sets of universe data, and may only be interested in controlling subsections of different universes.

While sACN has a priority field in the packet, this does not cover cases where a controller wants to explicitly control a small subsection of values without disturbing the rest – and HTP doesn’t help when that controller wants to force a subset of the values to a lower DMX level.

To better handle multiple source control scenarios, the 0xDD sACN start code was obtained by ETC from ESTA to allow setting a priority per-address, with the option of having DMX levels for a particular address ignored by a receiver.  This provides a more versatile and finer resolution priority mechanism over the sACN provided packet priority. This document details how sACN devices implementing this extension will use this start code.

# Document Conventions
The following acronyms and terms are used:
- *sACN*,  *Streaming ACN*, *Streaming DMX*: Reference "BSR E1.31 DMX512-A Streaming Protocol"
- *Receiver*: A device that receives sACN
- *Source*: A device that sends sACN. Sources are uniquely identified by their CID.
- *DMX level(s)*: Property values associated with a Null START code data packet.
- *HTP*: Highest Takes Precedence: The source with the highest level will have control.
- *Packet priority field*: Value of priority field in an sACN packet.
- *Packet options field*: Value of options field in an sACN packet.
- *Universe*: A collection of 512 values, corresponding to a DMX universe.
- *Address*: A single value in the universe, also known as a channel.
- *Universal Hold Last Look Time*: Configurable time after which source loss behavior will take effect.
- *Source loss Behavior*: Action taken by a receiver to a DMX level in the absence of a controlling source. This is typically
  - Assume 0
  - Fade to 0
  - Hold last known level for a preconfigured time (could be forever)
  - Fade to some predetermined level
  - Disable port or something similar as determined by the manufacture of the receiver.

# Rules for using the 0xDD start code
1. Devices will support per-address priority via the alternate start code 0xDD, which will be used to identify the sACN packet as carrying "per-address priority" values and not DMX levels.
2. The rules apply on a per universe basis where a receiver is receiving sACN packets for the same universe from different sources.
3. Per-address priority values consist of a series of single byte values indicating the source’s desired priority for the corresponding DMX level. Priority values can be set from 0 to 200 where 200 is the highest priority and 1 is the lowest priority.  Priority values 201 thru 255 are reserved for the receiver to use as internal priorities. 
4. A priority value of 0 indicates that the DMX level from the source is to be ignored by the receiver.
5. The final value at an address is taken from the source with highest priority. If there is more than one source with the same highest priority, HTP shall be applied between sources.
6. If a 0xDD packet is received with less than 512 values, the missing values are assumed to be 0 (DMX level ignored);
7. Per-address priorities for sources that do not send 0xDD packets will be taken from the packet priority field. 
8. The source follows the sACN rules for non-changing data for 0xDD packets as well, so a change of priorities for a universe is sent for 3 extra frames and then sent once every 800-1000ms. 
9. When a source wants to take control of an address, it should set desired DMX level and then set the per-address priority to a non-0 value.
10.	When a source wants to give up control of an address, it should set the per-address priority to zero. It is recommended to also set the DMX level to zero, because setting a priority to zero may result in source loss behavior for the corresponding address if there is no other source that gets control.
11. For some compatibility with receivers that don’t support the 0xDD packets, sources sending 0xDD packets shall also provide a packet priority value that applies to all addresses.  The default value for this priority shall be 100.  This allows for packet priority and HTP control between sources.
12. When a receiver detects a new source, it waits for a 0xDD packet for up to 1.5 seconds before processing DMX levels from that source.  If a receiver does not detect a 0xDD packet, it falls back and uses the packet priority until a 0xDD packet is detected.   
13.	When a receiver detects that a source has reached the sACN source loss timeout for 0xDD packets (e.g. no 0xDD packets in Universal Hold Last Look Time--2.5 seconds at minimum), but is still receiving NULL start code packets, the receiver shall fall back to using the sACN packet priority located in the NULL start code packets.
14. When a receiver detects that a source has reached the sACN source loss timeout for NULL start code packets (e.g. no NULL start code packets in the Universal Hold Last Look Time--2.5 seconds at minimum) the receiver shall act as if the source has given up control of its addresses as stated in 4.16 even if 0xDD packets continue to be received.  
The desired sequence for a source to terminate its sACN stream is to send three NULL start code packets with the Stream_Terminated bit (6) of the Packet options field set to 1.  This will allow the device with the next highest priority to gain control after the appropriate sampling time resolution.
15. When a receiver detects that a source has given up control of an address (by setting the priority for that address to 0), the receiver will give control to the source with next highest priority. If no source is available, source loss behavior for that address shall apply.

# Summary
In summary, priority may be specified on a per packet basis in sACN. This ETC extension allows priorities to be provided on a per-address basis using the alternate start code of 0xDD.

With this extension, taking control is accomplished by setting a non-zero priority for an address.  Giving up control is the reverse, setting the priority to 0.

Overall, receivers wait for a short period on start up to see if the alternate start code priority information is being provided before they start accepting data.

If no alternate start code priority information arrives, they use the packet priority.