# A new firmware for the Paikea buoy

An attempt to create a new firmware for Scout

## New message format

The message format has been extended. In a draft version, we still call the message PK001, and the extended version is fully compatible with the original PK001 and will be processed by the backend as is. 

Two fields have been added: 

- interval (int): confirms the current sending interval
  
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
