#ifndef PROTOCOL_H
#define PROTOCOL_H

/* tipos inteiros */
typedef __UINT8_TYPE__   uint8_t;
typedef __UINT16_TYPE__  uint16_t;
typedef __UINT32_TYPE__  uint32_t;
typedef __UINT64_TYPE__  uint64_t;
typedef long             ssize_t;
typedef unsigned int     socklen_t;

/* cabeçalho IP (RFC 791) */
struct iphdr {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint8_t  version:4;
    uint8_t  ihl:4;
#else
    uint8_t  ihl:4;
    uint8_t  version:4;
#endif
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};

/* cabeçalho UDP (RFC 768) */
struct udphdr {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
};

/* constantes de socket */
#define AF_INET       2
#define SOCK_RAW      3
#define IPPROTO_IP    0
#define IPPROTO_UDP   17
#define IPPROTO_RAW   255
#define IP_HDRINCL    3
#define MSG_DONTWAIT  0x40

/* endereços */
struct in_addr {
    uint32_t s_addr;
};

struct sockaddr_in {
    uint16_t       sin_family;
    uint16_t       sin_port;
    struct in_addr sin_addr;
    uint8_t        sin_zero[8];
};

struct sockaddr {
    uint16_t sa_family;
    uint8_t  sa_data[14];
};

/* temporização */
struct timeval {
    long tv_sec;
    long tv_usec;
};

/* errno */
extern int errno;
#define EAGAIN       11
#define EWOULDBLOCK  11

/* conversão de bytes */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define htons(x)  ((uint16_t)(x))
#  define htonl(x)  ((uint32_t)(x))
#else
#  define htons(x)  ((uint16_t)(((uint16_t)(x) >> 8) | ((uint16_t)(x) << 8)))
#  define htonl(x)  ((uint32_t)(                            \
       (((uint32_t)(x) & 0xFF000000U) >> 24) |              \
       (((uint32_t)(x) & 0x00FF0000U) >>  8) |              \
       (((uint32_t)(x) & 0x0000FF00U) <<  8) |              \
       (((uint32_t)(x) & 0x000000FFU) << 24)))
#endif
#define ntohs(x)  htons(x)
#define ntohl(x)  htonl(x)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define htonll(x)  ((uint64_t)(x))
#else
#  define htonll(x)  ((uint64_t)(                                   \
       ((uint64_t)htonl((uint32_t)((uint64_t)(x) & 0xFFFFFFFFULL)) << 32) | \
       ((uint64_t)htonl((uint32_t)((uint64_t)(x) >> 32)))))
#endif
#define ntohll(x) htonll(x)

/* syscalls */
#define SIGINT 2
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
void exit(int status);
int     socket(int domain, int type, int protocol);
int     setsockopt(int sockfd, int level, int optname,
                   const void *optval, socklen_t optlen);
ssize_t sendto(int sockfd, const void *buf, __SIZE_TYPE__ len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, __SIZE_TYPE__ len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
int     close(int fd);
int     usleep(unsigned int usec);
int     gettimeofday(struct timeval *tv, void *tz);

/* funções de protocol.c */
uint32_t inet_addr(const char *cp);

uint32_t inet_addr(const char *cp);
void *memset(void *s, int c, __SIZE_TYPE__ n);
void *memcpy(void *dst, const void *src, __SIZE_TYPE__ n);
int    atoi(const char *s);
double atof(const char *s);
#define RAND_MAX 0x7FFFFFFF
int  rand(void);
void srand(unsigned int seed);

/* protocolo */
#define MAX_DATA_SIZE   1448   /* MTU Ethernet 1500 - 20 IP - 8 UDP - 24 ProtoHeader */
#define WINDOW_SIZE     64     /* mais slots em voo = maior throughput */
#define TIMEOUT_SEC     0
#define TIMEOUT_USEC    500000 /* 500ms — menos retransmissoes desnecessarias */
#define MAX_SEQ         0xFFFFFFFF

/* flags */
#define FLAG_ACK  0x01
#define FLAG_SYN  0x02
#define FLAG_FIN  0x04
#define FLAG_DATA 0x08

/* portas */
#define SRC_PORT  54321
#define DST_PORT  12345

/* cabeçalho do protocolo */
typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint64_t seq_num;    /* 64 bits — suporta arquivos >4GB */
    uint64_t ack_num;
    uint8_t  flags;
    uint8_t  window;
    uint16_t checksum;
    uint16_t data_len;
    uint16_t padding;
} ProtoHeader;


typedef struct __attribute__((packed)) {
    ProtoHeader hdr;
    uint8_t     data[MAX_DATA_SIZE];
} Segment;

/* utilitários */
uint16_t compute_checksum(void *data, int len);
int      checksum_valid(Segment *seg);
void     build_segment(Segment *seg,
                    uint64_t seq, uint64_t ack,
                    uint8_t flags, uint8_t window,
                    const uint8_t *payload, uint16_t payload_len);

#endif /* PROTOCOL_H */
