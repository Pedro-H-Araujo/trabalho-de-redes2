# Trabalho de Redes 2 — Protocolo de Transporte Confiável

## Compilar

```bash
gcc -Wall -o sender sender.c protocol.c
gcc -Wall -o receiver receiver.c protocol.c
```

## Gerar arquivo de teste (10MB)

```bash
dd if=/dev/urandom of=teste.bin bs=1M count=10
```

## Sequência de execução

Sempre inicie o **receiver primeiro**, depois o **sender**.

**Terminal 1 — Receiver:**
```bash
sudo ./receiver 127.0.0.1 127.0.0.1 saida.bin <modelo> <semente> <p_loss>
```

**Terminal 2 — Sender:**
```bash
sudo ./sender 127.0.0.1 127.0.0.1 teste.bin
```

**Verificar integridade:**
```bash
md5sum teste.bin saida.bin
```
Os dois hashes devem ser iguais.

---

## Testes obrigatórios

### Sem perda
```bash
rm -f saida.bin && sudo ./receiver 127.0.0.1 127.0.0.1 saida.bin 0 42 0.0
sudo ./sender 127.0.0.1 127.0.0.1 teste.bin
md5sum teste.bin saida.bin
```

### 10% de perda (Bernoulli)
```bash
rm -f saida.bin && sudo ./receiver 127.0.0.1 127.0.0.1 saida.bin 0 42 0.1
sudo ./sender 127.0.0.1 127.0.0.1 teste.bin
md5sum teste.bin saida.bin
```

### 30% de perda (Bernoulli)
```bash
rm -f saida.bin && sudo ./receiver 127.0.0.1 127.0.0.1 saida.bin 0 42 0.3
sudo ./sender 127.0.0.1 127.0.0.1 teste.bin
md5sum teste.bin saida.bin
```

### Modelo bimodal
```bash
rm -f saida.bin && sudo ./receiver 127.0.0.1 127.0.0.1 saida.bin 1 42 0.0 0.3 0.8
sudo ./sender 127.0.0.1 127.0.0.1 teste.bin
md5sum teste.bin saida.bin
```

---

## Teste com arquivo grande

Verifique o espaço disponível antes:
```bash
df -h .
```

Crie um arquivo esparso do tamanho que couber (não ocupa espaço real em disco):
```bash
truncate -s 20G testeGrande.bin   # ajuste o tamanho conforme espaço disponível
```

```bash
rm -f saida.bin && sudo ./receiver 127.0.0.1 127.0.0.1 saida.bin 0 42 0.0
sudo ./sender 127.0.0.1 127.0.0.1 testeGrande.bin
md5sum testeGrande.bin saida.bin
```

---

## Parâmetros do receiver

| Parâmetro | Descrição |
|---|---|
| `src_ip` | IP do receiver (quem envia ACKs) |
| `dst_ip` | IP do sender |
| `saida` | Arquivo de saída |
| `modelo` | `0` = Bernoulli, `1` = Bimodal |
| `semente` | Semente do gerador aleatório |
| `p_loss` | Probabilidade de perda (0.0 a 1.0) |
| `p_bad` | *(bimodal)* Prob. de entrar no estado ruim |
| `p_loss_bad` | *(bimodal)* Prob. de perda no estado ruim |
