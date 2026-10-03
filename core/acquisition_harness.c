#include "acquisition.h"
#include <stdio.h>
#include <string.h>

/* Offline adapter only: same production C engine, deterministic virtual time. */
static bool send_frame(void *context, const md_can_frame_t *frame)
{
    (void)context;
    printf("TX %X ", (unsigned)frame->id);
    for (unsigned i=0; i<frame->dlc; ++i) printf("%02X", frame->data[i]);
    puts("");
    return true;
}

int main(void)
{
    md_acquisition_t engine;
    const md_acq_config_t config={.allow_standard_obd=true,.startup_ms=1500,.profile=NULL};
    md_acq_init(&engine,&config,send_frame,NULL,0);
    char line[256], command[16], hex[17];
    unsigned now,id,available;
    while (fgets(line,sizeof(line),stdin)) {
        if (sscanf(line,"%15s %u",command,&now)!=2) return 2;
        if (!strcmp(command,"tick")) md_acq_tick(&engine,now);
        else if (!strcmp(command,"rx")) {
            if (sscanf(line,"%15s %u %x %16s",command,&now,&id,hex)!=4 || strlen(hex)%2) return 2;
            md_can_frame_t frame={.id=id,.dlc=(uint8_t)(strlen(hex)/2)};
            for (unsigned i=0;i<frame.dlc;i++) {
                unsigned byte;
                if (sscanf(hex+2*i,"%2x",&byte)!=1) return 2;
                frame.data[i]=(uint8_t)byte;
            }
            md_acq_receive(&engine,&frame,now);
        } else if (!strcmp(command,"bus")) {
            if (sscanf(line,"%15s %u %u",command,&now,&available)!=3) return 2;
            md_acq_bus_available(&engine,available!=0,now);
        } else if (!strcmp(command,"dtc")) {
            printf("DTC_REQUEST %u\n",md_acq_request_dtc(&engine,now));
        } else if (!strcmp(command,"status")) {
            md_ui_signal_snapshot_t rpm=md_acq_snapshot(&engine,MD_SIGNAL_ENGINE_RPM,now,NULL);
            printf("STATUS %s %s %u %u %.0f\n",md_acq_state_name(engine.state),engine.vin,
                   engine.dtc_valid,engine.dtc_count,(double)rpm.value);
        } else return 2;
        puts("END");
        fflush(stdout);
    }
    return ferror(stdin) ? 2 : 0;
}
