# Per Channel Priority                                  {#per_channel_priority}


The sACN protocol specification details DMX-style control over TCP/IP networks.  While this provides a fast and efficient mechanism to transport the well-understood DMX protocol, it also introduces a problem that may not be as well-understood by the industry: multiple sources.

Unlike DMX over RS485, multiple sources may easily provide overlapping sets of universe data, and may only be interested in controlling subsections of different universes.  While sACN has a priority field in the packet, this does not cover cases where a controller wants to explicitly control a small subsection of values without disturbing the rest – and HTP doesn’t help when that controller wants to force a subset of the channels to a lower DMX level.

To better handle multiple source control scenarios, the 0xDD sACN start code was obtained by ETC from ESTA to allow setting a priority per-channel, with the option of having DMX levels for a particular channel ignored by a sink.  This provides a more versatile and finer resolution priority mechanism over the sACN provided packet priority. This document details how devices wishing to use this finer-grained mechanism should behave.


# References
ANSI E1.31: Lightweight streaming protocol for transport of DMX512 using ACN

# Document Conventions
The following acronyms and terms are used:
- *sACN*,  *Streaming ACN*,  *streaming DMX*: Reference "ANSI E1.31"
- *Sink*: A device that receives sACN
- *Source*: A device that send sACN. Sources are uniquely identified by their CID.
- *DMX level(s)*: Property values associated with a Null START code data packet.
- *HTP*: Highest Takes Precedence: The source with the highest level will have control.
- *Packet priority field*: Value of priority field in a sACN packet.
- *Final channel level*: Channel level after all rules have been applied.
- *Data loss behavior*: Action taken by a sink to a DMX level in the absence of a controlling source. This is typically one of
  - Assume 0
  - Fade to 0
  - Hold last known level for a preconfigured time (could be forever)
  - Fade to some predetermined level, e)disable port or something similar as determined by the manufacture of the sink.

# Rules for using the 0xDD start code
1. Devices will support per-channel priority via the alternate start code 0xDD, which will be used to identify the sACN packet as carrying “per-channel priority” values and not DMX levels.
2. The rules apply on a per universe basis where a sink is receiving sACN packets for the same universe from different sources.
3. Per-channel priority values consist of a series of single byte values indicating the source’s desired priority for the corresponding DMX level. Priority values can be set from 0 to 200 where 200 is the highest priority and 1 is the lowest priority.  Priority values 201 thru 255 are reserved for the sink to use as internal priorities. 
4. A priority value of 0 indicates that the DMX level from the source is to be ignored by the sink.
5. The final channel level is taken from the source with highest priority. If there is more than one source with the same highest priority, HTP shall be applied between sources.
6. If a 0xDD packet is received with less than 512 values, the missing values are assumed to be 0 (DMX level ignored);
7. Per-channel priorities for sources that do not send 0xDD packets will be taken from the packet priority field. If the packet priority field is zero (source does not support priorities), a value of 100 will be used.
8. The source follows the sACN rules for non-changing data for 0xDD packets as well, so a change of priorities for a universe is sent for 3 extra frames and then sent once every 800-1000ms. 
9. When a source wants to take control of a channel, it should set desired DMX level and then set the per-channel priority to a non-0 value.
10. When a source wants to give up control of a channel, it sets the per-channel priority to a 0 value.  It is recommended that it subsequently sends out DMX levels of 0 for that channel.
11. Setting a priority to zero may result in data loss behavior for the corresponding channel if there is no other source that gets control.  
It may be desirable for sources to provide a user configurable option to also set the data to 0 before setting the priority to 0.
12. For some compatibility with sinks that don’t support the 0xDD packets, sources sending 0xDD packets shall also provide a packet priority value that applies to all channels.  The default value for this priority shall be 100.  This allows for packet priority and HTP control between sources.
13. When a sink detects a new source, it waits for a 0xDD packet for up to 1.5 seconds before processing DMX levels from that source.  If a sink does not detect a 0xDD packet, it fails back and uses the packet priority until a 0xDD packet is detected.   
14. When a sink detects that a source has reached the sACN data loss timeout for 0xDD packets (e.g. no 0xDD packets in 2.5 seconds), but is still receiving NULL start code packets, the sink shall fall back to using the sACN packet priority located in the NULL start code packets.
15. When a sink detects that a source has reached the sACN data loss timeout for NULL start code packets (e.g. no NULL start code packets in 2.5 seconds) the sink shall act as if the source has given up control of its channels as stated in point 16 even it 0xDD packets continue to be received.  
The desired sequence for a source to terminate its sACN stream is to first send three 0xDD packets releasing control of all channels and then stop sending NULL start code and 0xDD packets.  This will allow the device with the next highest priority to gain control without having to wait for the 2.5 second timeout.
16. When a sink detects that a source has given up control of a channel (by setting the priority for that channel to 0), the sink will give control to the source with next highest priority. If no source is available, data loss behavior for that channel shall apply.

# Summary
In summary, priority may be specified on a per packet basis in sACN using this extension to the protocol.  This extension allows priorities to be provided on a per-channel basis using the alternate start code of 0xDD.

With this extension, take control is done via setting a non-zero priority for a channel.  Giving up control is the reverse, setting the priority to 0.   

Overall, sinks wait for a short period on start up to see if the alternate start code priority information is being provided before they start accepting data.

If no alternate start code priority information arrives, they use the packet priority.  If the packets specify that they are not carrying a priority, by placing a zero in the field, a priority of 100 is assumed for all channels.  
