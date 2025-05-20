# A new firmware for the Paikea buoy

An attempt to create a new firmware for Scout

## (Legacy v2) Message formats

PK001, Pk006, and Pk007 are still in use

### PK001 - FW v2 by Matt Arcady

Example: ```PK001;lat:3530.0000,NS:N,lon:12200.0000,EW:W,utc:191049.00,sog:0.000,cog:0,sta:00,batt:4.20```

```PK001;lat:{Latitude in NMEA format},NS:{N|S},lon:{Longitude},EW:{E|W},utc:{utc in NMEA},sog:{SOG},cog:{COG},sta:00,batt:{voltage}```

NMEA see https://www.gpsworld.com/what-exactly-is-gps-nmea-data/

SOG ... Speed over ground (unused or 0)
COG ... Course over ground (unused or 0)
sta ... status (unused)

### PK004 - FW v2 by Matt Arcady - obsolete

A message sent out by the deprecated handset

### PK005 - FW v2 Matt Arcady - obsolete

A message to put the buoy in terminal, debug, or update mode

### PK006 - Interval 
Example: ```+DATA:PK006,10;```

```+DATA:PK006,{minutes}```

**NOTE:**
- we are using minutes
- rather inconsistent format using +DATA and ;

### PK007 - Sleep
Example: ```+DATA:PK007,259200;```

```+DATA:PK007,{seconds}```

**NOTE:**
- we are using seconds here, INCONSISTENT!

## v3 message formats

PK006, and PK007 are still used as download messages to allow for compatible operation between v2 and v3

### PK101 (modified PK001)

This is a temporary format and might change before finalized 

Example: ```PK101;lat:3750.5119,NS:N,lon:12216.5280,EW:W,utc:194031.00,batt:3.74,int:10,sl:0,st:0```

```PK101;lat:{Latitude in NMEA format},NS:{N|S},lon:{Longitude},EW:{E|W},utc:{utc in NMEA},batt:{voltage},int:{interval_mins},sl:{time_mins},st:{status or mode}```

The message format has been extended. In a draft version, we call the message PK01, and the extended version is fully compatible with the original PK001 and will be processed by the backend as is. 

Three fields have been added: 

- interval (int): confirms the current sending interval in minutes

- sleeb (int}: sleep time in minutes
  
- status (st): indicates for what reason the message has been send

  - 0: normal message on schedule (on set schedule)
  - 1: first message after power on (immediately)
  - 2: retry off schedule (on set +10, +20, + 30 minutes, new scheduled message will supersede this and end this sequence)
  - 3: configuration change received and applied (immediately after receiving config change request, afterwards +10, +20, +30, or next successful scheduled message)
  - 4: error parsing incoming messages or requested config settings out of bound (immediately after receiving config change request, afterwards +10, +20, +30, or next successful scheduled message)
 
In addition to the status information, all messages will contain the full set of location information as well as the set schedule.

## Schedule

1. After power on: immediately send, than 10 minute interval (in the 10 minute interval there will be no retries), we will send :00, :10, :20, :30, :40), failed messages will be simply missing from that sequence
2. Longer send interval, e.g. 1 hour: We will try at :00, :10, :20, :30 and end the sequence when success
3. Intermediate interval, e.g. 20 min: We will try at :00, :10, since :20 would be the next scheduled message we end the sequence at :20
4. Super short interval "last mile" (e.g. 3 mins): A 5min interval should still work reliably, we expect an overlap between sending attempts and the next scheduled message below 5mins. While we sill try to send :03, :06, :09, the sequence might become somewaht unpredictable. We should still get the message out under 5mins and 2mins would be realistic under good conditions. But even a setting of 0 should not break the system.

## Deep sleep

I prefer to set the wakeup time for deep sleep relative to when the request is send. E.g. If someone requests 24 hours from 3:15 it should wake up at 3:15 the next day. Even better would be an absolute time request but I guess that needs to wait for later since the timer of the ESP32 is not super precise and can loose up to 30 minutes over a day.
