#include <string.h>
#include "protocol.h"

/* ──────────────────────────────────────────
   Checksum de 16 bits (complemento de 1)
   Mesmo algoritmo usado em IP/UDP/TCP.
   ────────────────────────────────────────── */
uint16_t compute_checksum(void *data, int len) {
    uint16_t *ptr = (uint16_t *)data;
    uint32_t  sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    /* byte sobrando (len ímpar) */
    if (len == 1)
        sum += *(uint8_t *)ptr;

    /* dobra o carry */
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)(~sum);
}

/* Valida checksum de um segmento recebido.
   Retorna 1 se OK, 0 se corrompido.         */
int checksum_valid(Segment *seg) {
    uint16_t received = seg->hdr.checksum;
    seg->hdr.checksum = 0;                   /* zera antes de recalcular */

    /* CORRIGIDO: ntohs() para converter de network byte order */
    int total = sizeof(ProtoHeader) + ntohs(seg->hdr.data_len);

    /* sanidade: evita leitura fora dos limites */
    if (total < (int)sizeof(ProtoHeader) || total > (int)sizeof(Segment)) {
        seg->hdr.checksum = received;
        return 0;
    }

    uint16_t calc = compute_checksum(seg, total);

    seg->hdr.checksum = received;            /* restaura */
    return (calc == received);
}

/* Monta um segmento completo pronto para envio. */
void build_segment(Segment *seg,
                   uint32_t seq, uint32_t ack,
                   uint8_t flags, uint8_t window,
                   const uint8_t *payload, uint16_t payload_len)
{
    memset(seg, 0, sizeof(Segment));

    seg->hdr.src_port  = htons(SRC_PORT);
    seg->hdr.dst_port  = htons(DST_PORT);
    seg->hdr.seq_num   = htonl(seq);
    seg->hdr.ack_num   = htonl(ack);
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