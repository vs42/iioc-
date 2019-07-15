# Documentation for IIOC++

##Buffer
class for buffer
###methods and properties:
method "begin" returns iterator for begin of the buffer
method "end" returns terator for end of the buffer
method "push" pushes the buffer
method "refill" refills the buffer

##Device
class for device
###methods and properties:
property "in" is map of incoming channels
property "out" is map of outgoing channels
property "attributes" is map of device attributes
method "id" returns id of device
method "name" returns name of device

##Context
class for context
has constructor by type
type uri
    make uri context by uri
type local
    make local context
type network
    make network context by ip
###methods and properties:
property "devices" is map of devices
method "name" returns name of context
method "devices_count" return number of devices

##Channel
class for channel
###methods and properties:
property "attributes" is map of channel attributes
method "id" returns id of channel
method "name" returns name of channel
method "enable" enables channel
method "disable" disables channel
