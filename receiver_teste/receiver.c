#include <stdio.h>
#include "protocol.h"

/* modelo de perda */
typedef enum { LOSS_BERNOULLI, LOSS_BIMODAL } LossModel;

typedef struct {
    LossModel model;
    double    p_loss;      /* Bernoulli: prob. de perda            */
    double    p_bad;       /* Bimodal: prob. de entrar em "bad"    */
    double    p_loss_bad;  /* Bimodal: prob. de perda no estado bad*/
    int       in_bad;      /* estado atual do modelo bimodal       */
} LossConfig;

static LossConfig loss_cfg;

/* Retorna 1 se o pacote deve ser DESCARTADO */
static int should_drop(void) {
    double r = (double)rand() / RAND_MAX;

    if (loss_cfg.model == LOSS_BERNOULLI) {
        return r < loss_cfg.p_loss;
    } else {
        /* Bimodal: alterna entre estado "good" e "bad" */
        if (loss_cfg.in_bad) {
            if (r < (1.0 - loss_cfg.p_bad))
                loss_cfg.in_bad = 0;
            return ((double)rand() / RAND_MAX) < loss_cfg.p_loss_bad;
        } else {
            if (r < loss_cfg.p_bad)
                loss_cfg.in_bad = 1;
            return 0; /* estado good: sem perda */
        }
    }
    return 0;
}

/* buffer de reordenação out-of-order */
#define REORDER_BUF 256

typedef struct {
    uint64_t seq;
    uint16_t len;
    uint8_t  data[MAX_DATA_SIZE];
    int      valid;
} ReorderSlot;

static ReorderSlot reorder_buf[REORDER_BUF];

static uint64_t total_received = 0;
static uint64_t total_dropped  = 0;
static uint64_t total_dup      = 0;

static void sigint_handler(int sig) {
    (void)sig;
    printf("\n[RECEIVER] Estatisticas:\n");
    printf("  Pacotes recebidos:   %llu\n", (unsigned long long)total_received);
    printf("  Pacotes descartados: %llu\n", (unsigned long long)total_dropped);
    printf("  Duplicatas:          %llu\n", (unsigned long long)total_dup);
    exit(0);
}

/* envia ACK */
static void send_ack(int sock_send,
                     const char *src_ip, const char *dst_ip,
                     struct sockaddr_in *dst,
                     uint64_t ack_num)
{
    uint8_t pkt[MAX_DATA_SIZE + 512];
    memset(pkt, 0, sizeof(pkt));

    struct iphdr  *iph  = (struct iphdr  *)pkt;
    struct udphdr *udph = (struct udphdr *)(pkt + sizeof(struct iphdr));
    Segment       *seg  = (Segment      *)(pkt + sizeof(struct iphdr) + sizeof(struct udphdr));

    /* monta segmento ACK */
    build_segment(seg, 0, ack_num, FLAG_ACK, WINDOW_SIZE, NULL, 0);

    int seg_size = sizeof(ProtoHeader);

    /* UDP */
    udph->source = htons(DST_PORT);
    udph->dest   = htons(SRC_PORT);
    udph->len    = htons(sizeof(struct udphdr) + seg_size);
    udph->check  = 0;

    /* IP */
    iph->ihl      = 5;
    iph->version  = 4;
    iph->tos      = 0;
    iph->tot_len  = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + seg_size);
    iph->id       = htons(1);
    iph->ttl      = 64;
    iph->protocol = IPPROTO_UDP;
    iph->check    = 0;
    iph->saddr    = inet_addr(src_ip);
    iph->daddr    = inet_addr(dst_ip);

    int pkt_len = ntohs(iph->tot_len);

    if (sendto(sock_send, pkt, pkt_len, 0,
               (struct sockaddr *)dst, sizeof(*dst)) < 0)
        perror("sendto ACK");
    else
        fprintf(stderr, "[ACK SENT] ack_num=%llu\n", (unsigned long long)ack_num);
}

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr,
            "Uso: %s <src_ip> <dst_ip> <saida> "
            "<modelo> <semente> <p_loss> [p_bad p_loss_bad]\n",
            argv[0]);
        return 1;
    }

    const char *src_ip   = argv[1];  /* IP do receptor (quem envia ACK) */
    const char *dst_ip   = argv[2];  /* IP do emissor                   */
    const char *outfile  = argv[3];
    int         model    = atoi(argv[4]);
    unsigned    seed     = (unsigned)atoi(argv[5]);
    double      p_loss   = atof(argv[6]);

    /* configura modelo de perda */
    loss_cfg.model    = (model == 0) ? LOSS_BERNOULLI : LOSS_BIMODAL;
    loss_cfg.p_loss   = p_loss;
    loss_cfg.in_bad   = 0;
    if (loss_cfg.model == LOSS_BIMODAL && argc >= 9) {
        loss_cfg.p_bad      = atof(argv[7]);
        loss_cfg.p_loss_bad = atof(argv[8]);
    }

    srand(seed);

    FILE *out = fopen(outfile, "wb");
    if (!out) { perror("fopen output"); return 1; }

    /* socket RAW para receber dados */
    int sock_recv = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sock_recv < 0) { perror("socket recv"); return 1; }

    /* socket RAW para enviar ACKs */
    int sock_send = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock_send < 0) { perror("socket send"); return 1; }
    int one = 1;
    setsockopt(sock_send, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    struct sockaddr_in dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family      = AF_INET;
    dst_addr.sin_addr.s_addr = inet_addr(dst_ip);

    memset(reorder_buf, 0, sizeof(reorder_buf));
    signal(SIGINT, sigint_handler);

    uint64_t expected_seq = 0;
    uint8_t buf[MAX_DATA_SIZE + 512];

    fprintf(stderr, "[RECEIVER] Aguardando pacotes...\n");

    while (1) {
        ssize_t r = recvfrom(sock_recv, buf, sizeof(buf), 0, NULL, NULL);
        if (r <= 0) continue;

        /* módulo de perda: antes de qualquer processamento */
        if (should_drop()) {
            total_dropped++;
            fprintf(stderr, "[DROP] pacote descartado (simulacao)\n");
            continue;
        }

        /* valida tamanho mínimo */
        if (r < (ssize_t)(sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(ProtoHeader)))
            continue;

        /* pula cabeçalho IP */
        struct iphdr *iph = (struct iphdr *)buf;
        int ip_hlen = iph->ihl * 4;

        /* valida ihl */
        if (ip_hlen < 20 || r < (ssize_t)(ip_hlen + sizeof(struct udphdr) + sizeof(ProtoHeader)))
            continue;

        /* pula UDP */
        struct udphdr *udph = (struct udphdr *)(buf + ip_hlen);
        int udp_offset = ip_hlen + sizeof(struct udphdr);

        /* filtra pela porta de destino */
        if (ntohs(udph->dest) != DST_PORT) continue;

        /* valida tamanho do segmento */
        if (r < (ssize_t)(udp_offset + sizeof(ProtoHeader)))
            continue;

        Segment *seg = (Segment *)(buf + udp_offset);

        /* filtra apenas pacotes DATA */
        if (!(seg->hdr.flags & FLAG_DATA)) continue;

        /* verifica checksum */
        if (!checksum_valid(seg)) {
            fprintf(stderr, "[ERROR] checksum inválido, descartando\n");
            continue;
        }

        uint64_t seq      = ntohll(seg->hdr.seq_num);
        uint16_t data_len = ntohs(seg->hdr.data_len);

        /* valida data_len */
        if (data_len == 0 || data_len > MAX_DATA_SIZE) continue;
        if (r < (ssize_t)(udp_offset + sizeof(ProtoHeader) + data_len)) continue;

        fprintf(stderr, "[RECV] seq=%llu len=%u expected=%llu\n",
                (unsigned long long)seq, data_len, (unsigned long long)expected_seq);

        /* ── detecção de duplicata ── */
        if (seq < (uint64_t)expected_seq) {
            total_dup++;
            fprintf(stderr, "[DUP] seq=%llu ja recebido, enviando ACK\n", (unsigned long long)seq);
            send_ack(sock_send, src_ip, dst_ip, &dst_addr, expected_seq);
            continue;
        }

        /* ── pacote em ordem ── */
        if (seq == (uint64_t)expected_seq) {
            fwrite(seg->data, 1, data_len, out);
            fflush(out);
            total_received++;
            expected_seq += data_len;

            int found = 1;
            while (found) {
                found = 0;
                for (int i = 0; i < REORDER_BUF; i++) {
                    if (reorder_buf[i].valid &&
                        reorder_buf[i].seq == (uint64_t)expected_seq)
                    {
                        fwrite(reorder_buf[i].data, 1,
                               reorder_buf[i].len, out);
                        fflush(out);
                        expected_seq += reorder_buf[i].len;
                        reorder_buf[i].valid = 0;
                        total_received++;
                        found = 1;
                        break;
                    }
                }
            }
            send_ack(sock_send, src_ip, dst_ip, &dst_addr, expected_seq);
        }
        /* ── pacote fora de ordem ── */
        else {
       /* verifica se já existe no buffer */
            int already = 0;
            for (int i = 0; i < REORDER_BUF; i++) {
                if (reorder_buf[i].valid && reorder_buf[i].seq == seq) {
                    already = 1;
                    break;
                }
            }
            if (!already) {
                for (int i = 0; i < REORDER_BUF; i++) {
                    if (!reorder_buf[i].valid) {
                        reorder_buf[i].seq   = seq;
                        reorder_buf[i].len   = data_len;
                        memcpy(reorder_buf[i].data, seg->data, data_len);
                        reorder_buf[i].valid = 1;
                        fprintf(stderr, "[OOO] seq=%llu guardado\n", (unsigned long long)seq);
                        break;
                    }
                }
            }
        }
    }
}