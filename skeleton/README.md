# PDFeditor skeleton

Server multi-protocol (UNIX, INET, SOAP, PDF+OCR) cu structura standard de proiect C.

## Structura

```
skeleton/
├── Makefile
├── include/          # proto.h, pdf_ocr.h, pdf_server.h, soap/sclient.h, proto_inet.h
├── src/
│   ├── main/         # threeds.c — singurul main()
│   ├── server/       # unixds, inetds, soapds, pdfds
│   ├── core/         # proto, proto_inet, pdf_ocr
│   └── client/       # pdfclient, pdfadmin, inetsample
├── build/obj/        # obiecte compilate
├── build/generated/soap/  # fisiere generate gSOAP
├── bin/              # executabile
├── config/           # server.cfg.example
├── data/input|output/
└── wsdl/
```

## Comenzi make

| Comanda | Descriere |
|---------|-----------|
| `make` / `make all` | serverds + toti clientii |
| `make serverds` | `bin/serverds` |
| `make pdfclient` | `bin/pdfclient` |
| `make pdfadmin` | `bin/pdfadmin` |
| `make inetclient` | `bin/inetclient` |
| `make soap` | regenereaza gSOAP (necesita `soapcpp2`) |
| `make clean` | sterge obiecte si binare |
| `make distclean` | clean + sterge SOAP generat |
| `make help` | ajutor |

## Compilare

```bash
cd skeleton
make distclean
make soap    # prima data, daca ai soapcpp2
make all
```

Config optional: `cp config/server.cfg.example config/server.cfg`

## Rulare

```bash
./bin/serverds [-c config/server.cfg] [-p 18083]
./bin/pdfclient /cale/absoluta/fisier.pdf
```

La pornire ar trebui sa vezi:

```
ocr_init: Tesseract ready, languages=ron+eng
[queue] Fir worker pornit (FIFO)
pdf_main: listening on port 18083 (unix: /tmp/admin.sock)
```

### Client PDF (REMOTE IN)

```bash
./bin/pdfclient -h 127.0.0.1 -p 18083 -f "$(pwd)/test.pdf" -o ocr -F txt -v
```

### Client admin (UX)

```bash
./bin/pdfadmin
# sau explicit:
./bin/pdfadmin -p /tmp/admin.sock
```

## Dependente (Ubuntu/Debian)

Vezi [README.md](../README.md) din radacina proiectului.
