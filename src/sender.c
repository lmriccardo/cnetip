#include <string.h>
#include "sender.h"


RawSender* RawSender_new(char *_srcaddr, char* _dstaddr, u_int16_t _dstport, char* _proto)
{
    struct sockaddr_in src, dst;
    int socketfd;

    struct protoent *proto = getprotobyname(_proto);
    socketfd = socket(AF_INET, SOCK_RAW, proto->p_proto);
    if (socketfd == -1) handle_error("socket");

    int value = 1;
    setsockopt(socketfd, IPPROTO_IP, IP_HDRINCL, &value, sizeof(value));

    /* Connect to the destination socket */
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(_dstport);
    inet_pton(AF_INET, _dstaddr, &dst.sin_addr);

    RawSender *sender = (RawSender*)malloc(sizeof(RawSender));
    sender->_srcaddress = _srcaddr;
    sender->_dstaddress = dst;
    sender->_socket = socketfd;
    sender->_msgcnt = 0;
    sender->_proto = proto;
    sender->_lastid = (unsigned short)getpid();
    sender->_icmpsn = 0;
}

void RawSender_delete(RawSender* _self)
{
    shutdown(_self->_socket, 2);
    free(_self);
}

void __RawSender_sendto_v2(RawSender* _self, char* _buffer, size_t _size)
{
    socklen_t dstlen;
    dstlen = sizeof(struct sockaddr_in);
    
    if (
        sendto(
            _self->_socket, _buffer, _size, 0, 
            (struct sockaddr *)&_self->_dstaddress, dstlen
        ) < 0
    ) {
        handle_error("sendto");
    }

    printf("Sent %ld bytes of IP Packet\n", _size);
}

void RawSender_sendto(RawSender* _self, IpPacket* _pckt)
{
    ByteBuffer* buffer = IpPacket_encode(_pckt);
    __RawSender_sendto_v2(_self, buffer->_buffer, buffer->_size);
    _self->_buff = buffer;
    _self->_msgcnt += 1;
}

void RawSender_printInfo(RawSender* _self)
{
    u_int16_t dstport = ntohs(_self->_dstaddress.sin_port);
    char *dstaddr = RawSender_getDestinationIP(_self);

    printf("[*] Sending To %s:%d\n", dstaddr, dstport);
}

char* RawSender_getDestinationIP(RawSender* _self)
{
    char *dstaddr = addressNumberToString(_self->_dstaddress.sin_addr.s_addr, true);
    return dstaddr;
}

void RawSender_sendc(RawSender* _self, IpPacket* _pckt)
{
    RawSender_printInfo(_self);

    while (1)
    {
        RawSender_sendto(_self, _pckt);
    }
}

IpPacket* RawSender_craftIpPacket(RawSender *_self, u_int16_t _id)
{
    u_int8_t proto = _self->_proto->p_proto;
    char *dstaddr = RawSender_getDestinationIP(_self);
    char *srcaddr = _self->_srcaddress;

    IpPacket* ippckt = craftIpPacket(
        IPv4, 0x0, 0x0, IP_HEADER_SIZE, _id, X_FLAG_NOT_SET, D_FLAG_SET,
        M_FLAG_NOT_SET, 0x0, TTL_DEFAULT_VALUE, proto, 0x0,
        srcaddr, dstaddr
    );

    return ippckt;
}

void RawSender_sendIcmp_Echo_v2(RawSender* _self, u_int8_t _type, u_int8_t _code, u_int16_t _id)
{
    RawSender_sendIcmp_Echo_v4(_self, _type, _code, _id, _self->_icmpsn++);
}

void RawSender_sendIcmp_Echo_v3(RawSender* _self, u_int8_t _type, u_int8_t _code, u_int16_t _seqnum)
{
    RawSender_sendIcmp_Echo_v4(_self, _type, _code, _self->_lastid, _seqnum);
}

void RawSender_sendIcmp_Echo_v4(
    RawSender* _self, u_int8_t _type, u_int8_t _code, u_int16_t _id, u_int16_t _seqnum
) {
    IpPacket* ippckt = RawSender_craftIpPacket(_self, _id);
    IcmpPacket *icmppckt = IcmpPacket_new_v2(_type, 0);

    IcmpPacket_fillHeader();
}

void RawSender_sendIcmp(RawSender* _self, u_int8_t _type, u_int8_t _code)
{
    RawSender_sendIcmp_v4(_self, _type, _code, _self->_lastid++, _self->_icmpsn++);
}

IpPacket* craftIpPacket(
    u_int8_t _version, int       _dscp,     int   _ecn,     u_int16_t _tlen,   u_int16_t _id,
    int      _xf,      int       _df,       int   _mf,      int       _offset, u_int8_t  _ttl,
    u_int8_t _proto,   u_int16_t _checksum, char* _srcaddr, char*     _dstaddr
) {
    IpPacket* ippckt = IpPacket_new();

    u_int16_t flagoff = computeFlagOff(_xf, _df, _mf, _offset);
    u_int16_t dsf = computeDifferentiatedServiceField(_dscp, _ecn);

    IpPacket_fillHeader(
        ippckt, _version, dsf, _tlen, _id, flagoff, _ttl, _proto,
        _checksum, _srcaddr, _dstaddr
    );

    return ippckt;
}