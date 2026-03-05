#include <stdio.h>
#include "protocol.h"


void *memset(void *s, int c, __SIZE_TYPE__ n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dst, const void *src, __SIZE_TYPE__ n) {
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}


uint32_t inet_addr(const char *cp) {
    uint32_t result = 0;
    int      shift  = 0;
    int      octet  = 0;
    int      dots   = 0;

    for (int i = 0; cp[i] != '\0' && dots <= 3; i++) {
        char c = cp[i];
        if (c >= '0' && c <= '9') {
            octet = octet * 10 + (c - '0');
        } else if (c == '.') {
            result |= (uint32_t)(octet & 0xFF) << shift;
            shift += 8;
            octet = 0;
            dots++;
        } else {
            return 0xFFFFFFFF; /* inválido */
        }
    }
    result |= (uint32_t)(octet & 0xFF) << shift;
    return result;
}


int atoi(const char *s) {
    int n    = 0;
    int sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (*s++ - '0');
    return sign * n;
}

double atof(const char *s) {
    double result = 0.0;
    double frac   = 0.0;
    double div    = 1.0;
    int    sign   = 1;
    int    in_frac = 0;

    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;

    while (*s != '\0') {
        if (*s >= '0' && *s <= '9') {
            if (in_frac) {
                div    *= 10.0;
                frac   += (*s - '0') / div;
            } else {
                result  = result * 10.0 + (*s - '0');
            }
        } else if (*s == '.') {
            in_frac = 1;
        } else {
            break;
        }
        s++;
    }
    return sign * (result + frac);
}


static unsigned long long _rand_state = 1;

void srand(unsigned int seed) {
    _rand_state = seed;
}

int rand(void) {
    _rand_state = _rand_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (int)((_rand_state >> 33) & 0x7FFFFFFF);
}

/* checksum de 16 bits */
uint16_t compute_checksum(void *data, int len) {
    uint16_t *ptr = (uint16_t *)data;
    uint32_t  sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1)
        sum += *(uint8_t *)ptr;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)(~sum);
}


int checksum_valid(Segment *seg) {
    uint16_t received = seg->hdr.checksum;
    seg->hdr.checksum = 0;
    int total = sizeof(ProtoHeader) + ntohs(seg->hdr.data_len);
    if (total < (int)sizeof(ProtoHeader) || total > (int)sizeof(Segment)) {
        seg->hdr.checksum = received;
        return 0;
    }

    uint16_t calc = compute_checksum(seg, total);

    seg->hdr.checksum = received;
    return (calc == received);
}


void build_segment(Segment *seg,
                   uint64_t seq, uint64_t ack,
                   uint8_t flags, uint8_t window,
                   const uint8_t *payload, uint16_t payload_len)
{
    memset(seg, 0, sizeof(Segment));

    seg->hdr.src_port  = htons(SRC_PORT);
    seg->hdr.dst_port  = htons(DST_PORT);
    seg->hdr.seq_num   = htonll(seq);
    seg->hdr.ack_num   = htonll(ack);
    seg->hdr.flags     = flags;
    seg->hdr.window    = window;
    seg->hdr.data_len  = htons(payload_len);
    seg->hdr.checksum  = 0;

    if (payload && payload_len > 0)
        memcpy(seg->data, payload, payload_len);

    /* calcula checksum sobre header + dados */
    int total = sizeof(ProtoHeader) + payload_len;
    seg->hdr.checksum = compute_checksum(seg, total);
}