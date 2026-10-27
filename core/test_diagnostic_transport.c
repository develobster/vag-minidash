#include "diagnostic_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"TP FAIL %d: %s\n",__LINE__,#x);exit(1);} }while(0)
int main(void)
{
    md_tp_flow_control_t fc;
    uint8_t control[]={0x30,8,0xf1};
    CHECK(md_tp_parse_flow_control(control,3,&fc) && fc.block_size==8 && fc.stmin_us==100);
    control[2]=0x7f;CHECK(md_tp_parse_flow_control(control,3,&fc) && fc.stmin_us==127000);
    control[0]=0x31;control[2]=0xf9;CHECK(md_tp_parse_flow_control(control,3,&fc) && fc.status==1 && fc.stmin_us==900);
    control[0]=0x32;CHECK(md_tp_parse_flow_control(control,3,&fc) && fc.status==2);
    control[0]=0x33;CHECK(!md_tp_parse_flow_control(control,3,&fc));
    control[0]=0x30;control[2]=0x80;CHECK(!md_tp_parse_flow_control(control,3,&fc));
    control[2]=0xf0;CHECK(!md_tp_parse_flow_control(control,3,&fc));
    control[2]=0xfa;CHECK(!md_tp_parse_flow_control(control,3,&fc));
    CHECK(!md_tp_parse_flow_control(control,2,&fc));
    CHECK(!md_tp_parse_flow_control(NULL,3,&fc));
    CHECK(!md_tp_expired(NULL,0));
    md_tp_rx_t r;md_tp_reset(&r);
    md_can_frame_t f={.id=0x7e8,.dlc=4,.data={3,0x7f,0x22,0x31}};
    CHECK(md_tp_receive(&r,&f,0)==MD_TP_COMPLETE && r.used==3 && r.data[2]==0x31);
    f.dlc=3;CHECK(md_tp_receive(&r,&f,0)==MD_TP_INVALID);
    f.dlc=0;CHECK(md_tp_receive(&r,&f,0)==MD_TP_INVALID);
    f.dlc=9;CHECK(md_tp_receive(&r,&f,0)==MD_TP_INVALID);
    md_tp_reset(&r);f=(md_can_frame_t){.id=0x7e8,.dlc=8,.data={0x10,16,0x62,0xf1,0x87,1,2,3}};
    CHECK(md_tp_receive(&r,&f,0)==MD_TP_FLOW);
    f=(md_can_frame_t){.id=0x7e8,.dlc=7,.data={0x21,4,5,6,7,8,9,10}};
    CHECK(md_tp_receive(&r,&f,1)==MD_TP_INVALID); /* truncated intermediate CF */
    f.dlc=8;CHECK(md_tp_receive(&r,&f,1)==MD_TP_MORE);
    f=(md_can_frame_t){.id=0x7e8,.dlc=3,.data={0x22,11,12,13}};
    CHECK(md_tp_receive(&r,&f,2)==MD_TP_INVALID); /* missing final payload byte */
    f.dlc=4;CHECK(md_tp_receive(&r,&f,2)==MD_TP_COMPLETE);
    CHECK(r.used==16 && r.data[15]==13);
    CHECK(md_tp_receive(&r,&f,3)==MD_TP_INVALID); /* orphan CF */
    md_tp_reset(&r);f=(md_can_frame_t){.id=0x7e8,.dlc=7,.data={0x10,8,0x62,0xf1,0x87,1,2,3}};
    CHECK(md_tp_receive(&r,&f,0)==MD_TP_INVALID); /* FF needs a full CAN data field */
    f.dlc=8;CHECK(md_tp_receive(&r,&f,UINT32_MAX-100)==MD_TP_FLOW);
    f=(md_can_frame_t){.id=0x7e8,.dlc=3,.data={0x21,4,5}};
    CHECK(!md_tp_expired(&r,148));CHECK(md_tp_expired(&r,149));
    CHECK(md_tp_receive(&r,&f,149)==MD_TP_INVALID);
    md_tp_reset(&r);f=(md_can_frame_t){.id=0x7e8,.dlc=8,.data={0x10,128}};
    CHECK(md_tp_receive(&r,&f,0)==MD_TP_FLOW);
    for(unsigned i=1;i<=18;i++) {
        f.data[0]=0x20|(i&15);
        CHECK(md_tp_receive(&r,&f,i)==(i==18?MD_TP_COMPLETE:MD_TP_MORE));
    }
    CHECK(r.used==128); /* includes sequence number wrap 15 -> 0 */
    md_tp_reset(&r);f.data[0]=0x10;f.data[1]=20;
    CHECK(md_tp_receive(&r,&f,0)==MD_TP_FLOW);
    f.data[0]=0x22;CHECK(md_tp_receive(&r,&f,1)==MD_TP_INVALID); /* missing CF1 */
    md_tp_reset(&r);f.data[0]=0x10;f.data[1]=129;
    CHECK(md_tp_receive(&r,&f,0)==MD_TP_INVALID);
    puts("TP PASS: padded/unpadded frames, truncation, bounded payload and N_Cr timeout");
    return 0;
}
