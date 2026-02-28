#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

/* ── Tamanhos e limites ── */
#define MAX_DATA_SIZE   1400   /* payload máximo por segmento        */
#define WINDOW_SIZE     16     /* tamanho da janela deslizante        */
#define TIMEOUT_SEC     0      /* timeout de retransmissão (segundos) */
#define TIMEOUT_USEC    200000 /* timeout de retransmissão (microsegundos) */
#define MAX_SEQ         0xFFFFFFFF

/* ── Flags do protocolo ── */
#define FLAG_ACK  0x01
#define FLAG_SYN  0x02
#define FLAG_FIN  0x04
#define FLAG_DATA 0x08

/* ── Portas padrão ── */
#define SRC_PORT  54321
#define DST_PORT  12345

/* ────────────────────────────────────────────
   Cabeçalho do protocolo customizado (20 bytes)
   ──────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t src_port;   /* porta de origem              */
    uint16_t dst_port;   /* porta de destino             */
    uint32_t seq_num;    /* número de sequência          */
    uint32_t ack_num;    /* número de confirmação        */
    uint8_t  flags;      /* ACK | SYN | FIN | DATA       */
    uint8_t  window;     /* tamanho da janela            */
    uint16_t checksum;   /* checksum de 16 bits          */
    uint16_t data_len;   /* tamanho do payload           */
    uint16_t padding;    /* alinhamento                  */
} ProtoHeader;

/* Segmento completo = cabeçalho + payload */
typedef struct __attribute__((packed)) {
    ProtoHeader hdr;
    uint8_t     data[MAX_DATA_SIZE];
} Segment;

/* ── Protótipos de utilidades ── */
uint16_t compute_checksum(void *data, int len);
int      checksum_valid(Segment *seg);
void     build_segment(Segment *seg,
                        uint32_t seq, uint32_t ack,
                        uint8_t flags, uint8_t window,
                        const uint8_t *payload, uint16_t payload_len);

#endif /* PROTOCOL_H */
