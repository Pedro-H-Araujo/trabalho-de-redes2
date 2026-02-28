#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/time.h>
#include <errno.h>
#include "protocol.h"

static uint64_t total_sent       = 0;
static uint64_t total_retransmit = 0;
static uint64_t total_acks       = 0;

typedef struct {
    uint8_t        pkt[4096];
    int            pkt_len;
    uint32_t       seq;
    uint32_t       seq_end;
    int            in_use;
    struct timeval sent_at;
} WindowSlot;

static WindowSlot window[WINDOW_SIZE];

static int build_raw_packet(uint8_t *buf,
                             const char *src_ip, const char *dst_ip,
                             const Segment *seg, uint16_t seg_size)
{
    struct iphdr  *iph  = (struct iphdr  *)buf;
    struct udphdr *udph = (struct udphdr *)(buf + sizeof(struct iphdr));
    uint8_t       *data  = buf + sizeof(struct iphdr) + sizeof(struct udphdr);

    memcpy(data, seg, seg_size);

    udph->source = htons(SRC_PORT);
    udph->dest   = htons(DST_PORT);
    udph->len    = htons(sizeof(struct udphdr) + seg_size);
    udph->check  = 0;

    iph->ihl      = 5;
    iph->version  = 4;
    iph->tos      = 0;
    iph->tot_len  = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + seg_size);
    iph->id       = htons(54321);
    iph->frag_off = 0;
    iph->ttl      = 64;
    iph->protocol = IPPROTO_UDP;
    iph->check    = 0;
    iph->saddr    = inet_addr(src_ip);
    iph->daddr    = inet_addr(dst_ip);

    return ntohs(iph->tot_len);
}

static uint32_t try_recv_acks(int sock_recv, uint32_t base_seq)
{
    uint8_t buf[4096];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    uint32_t new_base = base_seq;

    while (1) {
        ssize_t r = recvfrom(sock_recv, buf, sizeof(buf), MSG_DONTWAIT,
                             (struct sockaddr *)&from, &from_len);
        if (r <= 0) break;

        if (r < (ssize_t)(sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(ProtoHeader)))
            continue;

        struct iphdr *iph = (struct iphdr *)buf;
        int ip_hlen = iph->ihl * 4;
        if (ip_hlen < 20) continue;

        struct udphdr *udph = (struct udphdr *)(buf + ip_hlen);
        if (ntohs(udph->dest) != SRC_PORT) continue;

        int udp_offset = ip_hlen + sizeof(struct udphdr);
        if (r < (ssize_t)(udp_offset + sizeof(ProtoHeader))) continue;

        Segment *seg = (Segment *)(buf + udp_offset);
        if (!(seg->hdr.flags & FLAG_ACK)) continue;
        if (!checksum_valid(seg))          continue;

        uint32_t ack = ntohl(seg->hdr.ack_num);
        total_acks++;
        fprintf(stderr, "[ACK] ack_num=%u\n", ack);

        if (ack > new_base)
            new_base = ack;
    }
    return new_base;
}

static void check_timeouts(int sock_send, struct sockaddr_in *dst)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    for (int i = 0; i < WINDOW_SIZE; i++) {
        if (!window[i].in_use) continue;

        long elapsed_us =
            (now.tv_sec  - window[i].sent_at.tv_sec)  * 1000000L +
            (now.tv_usec - window[i].sent_at.tv_usec);

        long timeout_us = TIMEOUT_SEC * 1000000L + TIMEOUT_USEC;

        if (elapsed_us >= timeout_us) {
            sendto(sock_send, window[i].pkt, window[i].pkt_len, 0,
                   (struct sockaddr *)dst, sizeof(*dst));
            gettimeofday(&window[i].sent_at, NULL);
            total_retransmit++;
            fprintf(stderr, "[RETRANSMIT] seq=%u\n", window[i].seq);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <src_ip> <dst_ip> <arquivo>\n", argv[0]);
        return 1;
    }
    const char *src_ip   = argv[1];
    const char *dst_ip   = argv[2];
    const char *filename = argv[3];

    FILE *fp = fopen(filename, "rb");
    if (!fp) { perror("fopen"); return 1; }

    int sock_send = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock_send < 0) { perror("socket send"); return 1; }
    int one = 1;
    setsockopt(sock_send, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    int sock_recv = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sock_recv < 0) { perror("socket recv"); return 1; }

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family      = AF_INET;
    dst.sin_addr.s_addr = inet_addr(dst_ip);

    memset(window, 0, sizeof(window));

    uint32_t base_seq = 0;
    uint32_t next_seq = 0;
    int      eof      = 0;

    struct timeval start, end;
    gettimeofday(&start, NULL);

    while (!eof || base_seq < next_seq) {

        /* envia novos segmentos enquanto houver slot livre */
        while (!eof) {
            int in_flight = 0;
            for (int i = 0; i < WINDOW_SIZE; i++)
                if (window[i].in_use) in_flight++;
            if (in_flight >= WINDOW_SIZE) break;

            uint8_t payload[MAX_DATA_SIZE];
            size_t n = fread(payload, 1, MAX_DATA_SIZE, fp);
            if (n == 0) { eof = 1; break; }

            int slot = -1;
            for (int i = 0; i < WINDOW_SIZE; i++)
                if (!window[i].in_use) { slot = i; break; }
            if (slot < 0) break;

            Segment seg;
            build_segment(&seg, next_seq, 0, FLAG_DATA, WINDOW_SIZE,
                          payload, (uint16_t)n);

            int pkt_len = build_raw_packet(window[slot].pkt,
                                            src_ip, dst_ip,
                                            &seg, sizeof(ProtoHeader) + n);

            sendto(sock_send, window[slot].pkt, pkt_len, 0,
                   (struct sockaddr *)&dst, sizeof(dst));

            window[slot].pkt_len = pkt_len;
            window[slot].seq     = next_seq;
            window[slot].seq_end = next_seq + (uint32_t)n;
            window[slot].in_use  = 1;
            gettimeofday(&window[slot].sent_at, NULL);

            total_sent++;
            fprintf(stderr, "[SEND] seq=%u len=%zu\n", next_seq, n);
            next_seq += (uint32_t)n;
        }

        /* recebe ACKs */
        uint32_t new_base = try_recv_acks(sock_recv, base_seq);
        if (new_base > base_seq) {
            for (int i = 0; i < WINDOW_SIZE; i++)
                if (window[i].in_use && window[i].seq_end <= new_base)
                    window[i].in_use = 0;
            base_seq = new_base;
        }

        check_timeouts(sock_send, &dst);
        usleep(100);
    }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_usec - start.tv_usec) / 1e6;

    printf("\n═══════════════════════════════\n");
    printf("  Transmissão concluída!\n");
    printf("  Pacotes enviados:    %lu\n", total_sent);
    printf("  Retransmissões:      %lu\n", total_retransmit);
    printf("  ACKs recebidos:      %lu\n", total_acks);
    printf("  Tempo total:         %.2f s\n", elapsed);
    printf("═══════════════════════════════\n");

    fclose(fp);
    close(sock_send);
    close(sock_recv);
    return 0;
}
