#include "captive_dns.h"
#include <string.h>
size_t md_captive_dns_reply(uint8_t *p, size_t n, size_t capacity)
{
    if (!p || n<17 || n>capacity || (p[2]&0xf8) || p[4] || p[5]!=1) return 0;
    size_t pos=12;
    unsigned labels=0;
    while(pos<n && p[pos]) {
        unsigned len=p[pos++];
        if(len>63 || ++labels>127 || pos+len>=n) return 0;
        pos+=len;
    }
    if(pos+5!=n || p[pos+1] || p[pos+2]!=1 || p[pos+3] || p[pos+4]!=1 || n+16>capacity) return 0;
    p[2]=0x81; p[3]=0x80; p[6]=0; p[7]=1;
    memset(p+8,0,4);
    const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,30,0,4,192,168,4,1};
    memcpy(p+n,answer,sizeof(answer));
    return n+sizeof(answer);
}
#ifdef ESP_PLATFORM
#include "lwip/sockets.h"
#include <fcntl.h>
#include <unistd.h>
int md_captive_dns_open(void)
{
    int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(fd<0) return -1;
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(53)};
    address.sin_addr.s_addr=inet_addr("192.168.4.1");
    if(bind(fd,(struct sockaddr *)&address,sizeof(address))<0 || fcntl(fd,F_SETFL,O_NONBLOCK)<0) {
        close(fd); return -1;
    }
    return fd;
}
void md_captive_dns_poll(int fd)
{
    if(fd<0) return;
    /* Bound work even under a busy client; diagnostic/UI tasks keep running. */
    for(unsigned i=0;i<4;i++) {
        uint8_t packet[512]; struct sockaddr_in peer; socklen_t size=sizeof(peer);
        int n=recvfrom(fd,packet,sizeof(packet)-16,0,(struct sockaddr *)&peer,&size);
        if(n<=0) break;
        size_t reply=md_captive_dns_reply(packet,(size_t)n,sizeof(packet));
        if(reply) (void)sendto(fd,packet,reply,0,(struct sockaddr *)&peer,size);
    }
}
void md_captive_dns_close(int fd) { if(fd>=0) close(fd); }
bool md_captive_dns_selftest(void)
{
    int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(fd<0) return false;
    struct timeval timeout={.tv_sec=2};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(53)};
    address.sin_addr.s_addr=inet_addr("192.168.4.1");
    const uint8_t query[]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,4,'t','e','s','t',0,0,1,0,1};
    uint8_t reply[64];
    int n=-1;
    if(sendto(fd,query,sizeof(query),0,(struct sockaddr *)&address,sizeof(address))==(int)sizeof(query))
        n=recv(fd,reply,sizeof(reply),0);
    close(fd);
    return n==(int)sizeof(query)+16 && reply[0]==0x12 && reply[1]==0x34 &&
        reply[n-4]==192 && reply[n-3]==168 && reply[n-2]==4 && reply[n-1]==1;
}
#endif
