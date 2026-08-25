#ifndef WIFI_STA_H
#define WIFI_STA_H

/* Advances the join state machine (channel-lock -> open-system auth -> assoc)
   and retransmits the pending request on timeout. Called once per servicer
   tick. */
void wifi_station_tick(void);

/* Delivers a received frame (RxControl header included, exactly as the RX
   servicer holds it) to the join state machine. Frames not addressed to us
   from our target AP are ignored. */
void wifi_sta_input(volatile unsigned char *buf, unsigned int len);

enum {
    STA_INIT,
    STA_AUTH,
    STA_ASSOC,
    STA_RUN,
};

extern volatile int wifi_sta_state;
extern volatile unsigned int wifi_sta_aid;

#endif
