# Data Loss Behavior                                      {#data_loss_behavior}

The [sACN Receiver](@ref sacn_receiver) module uses a custom algorithm, designed by ETC's network
team, to react predictably in the event that multiple sACN Sources are lost in quick succession.

Consider the following scenario:
~~~
+----------------+  +----------------+
| Source 1       |  | Source 2       |           +----------+
| Priority: 90   |  | Priority: 100  |           | Receiver |
| All slots 0xff |  | All slots 0x00 |           +----------+
+----------------+  +----------------+                ||
        ||                  ||                        ||
      ====================================================
      |                  Network switch                  |
      ====================================================
~~~

Sources 1 and 2 are sending sACN consistently with the parameters shown on the same universe, to
which the receiver is listening. Then, the receiver is disconnected from the switch. The receiver
starts a data loss timer for both sources.

The timer for Source 2 could easily expire first, in which case the receiver will for a brief time
consider Source 1 to have taken over. This will result in a short flash of all of the DMX slots on
that universe to full, before the data loss timer for Source 1 also expires.

To avoid this unwanted behavior, the algorithm is specified as follows:

 * Whenever a source is considered to be lost, either due to the Stream_Terminated bit being set or
   a data loss timeout, the library shall compile a list of all other sources currently being
   tracked for that universe and wait until their online/offline status has been verified before
   sending a [sources_lost()](@ref SacnReceiverCallbacks::sources_lost) notification.
 * If multiple sources are determined to be offline during this verification, they shall all be
   included in the same [sources_lost()](@ref SacnReceiverCallbacks::sources_lost) notification so
   that the application can react to them simultaneously.

This results in a time period, hereafter referred to as a **settling time**, after a source has
been determined to be lost, during which the online/offline status of all other sources is
verified. This settling time runs concurrently with the **expired notification wait time**, which
is a global option settable through [sacnrecv_set_expired_wait()](@ref sacnrecv_set_expired_wait()).
The settling time could range from almost instantaneous to 2.5 seconds, and thus could be longer or
shorter than the expired notification wait time. The
[sources_lost()](@ref SacnRecvCallbacks::sources_lost) notification is not sent until both time
periods expire.

In this example, a source is lost due to timeout, and the settling time is shorter than the expired
notification wait time:
~~~
Source stops                      Source                               Application
sending data                     times out                               notified
     +                               +                                      +
     |                               |                                      |
     |------------------------------>|                                      |
     |    sACN Data Loss Timeout     |---------------------------->         |
     |          2.5 seconds          |       Settle Period                  |
     |                               |      0 to 2.5 seconds                |
     |                               |                                      |
     |                               |------------------------------------->|
     |                               |       Notification Wait Period       |
     |                               |    User-settable, default 1 second   |
~~~

Here is another example where a source is lost due to termination, and the settling time is longer
than the expired notification wait time:
~~~
   Source sets                                     Application
Stream_Terminated                                   notified
       +                                               +
       |                                               |
       |                                               |
       |---------------------------------------------->|
       |                Settle Period                  |
       |               0 to 2.5 seconds                |
       |                                               |
       |------------------------------------->         |
       |       Notification Wait Period                |
       |    User-settable, default 1 second            |
~~~
