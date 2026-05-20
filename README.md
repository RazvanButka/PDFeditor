# PDFeditor -- Server Multi-Protocol cu OCR

Server multi-threaded care expune servicii de procesare PDF prin 4 interfete paralele: UNIX socket, INET TCP, SOAP si PDF TCP. Include recunoastere text prin Tesseract integrat cu MuPDF.

## Cerinte sistem

- Ubuntu (sau Docker/Dev Container)
- GCC + Make + gSOAP (`soapcpp2`)

## Instalare dependente

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    gsoap libgsoap-dev \
    libconfig-dev \
    libmupdf-dev libmujs-dev \
    libfreetype-dev libharfbuzz-dev libgumbo-dev \
    libjpeg-dev libopenjp2-7-dev libjbig2dec0-dev \
    libtesseract-dev libleptonica-dev \
    tesseract-ocr tesseract-ocr-ron tesseract-ocr-eng \
    libncurses-dev \
    poppler-utils
```

## Compilare

```bash
cd skeleton/
make distclean
make soap    # daca ai soapcpp2; altfel foloseste fisierele din build/generated/soap
make all
```

## Rulare

### Terminal 1 — server

```bash
cd skeleton
./bin/serverds
# sau cu config:
./bin/serverds -c config/server.cfg
```

La pornire:

```
ocr_init: Tesseract ready, languages=ron+eng
pdf_main: listening on port 18083 (unix: /tmp/admin.sock)
```

### Terminal 2 — client

```bash
./bin/pdfclient /cale/completa/catre/fisier.pdf
```

Foloseste cale absoluta. PDF-ul trebuie sa existe pe masina serverului.

## Test rapid

```bash
sudo apt install -y enscript ghostscript

cat > /tmp/test.txt << 'EOF'
Ion Popescu
CNP: 1820123456789
Email: ion@example.com
EOF

enscript /tmp/test.txt -o - | ps2pdf - /tmp/test.pdf
./bin/pdfclient /tmp/test.pdf
```

## Structura proiect

Vezi [skeleton/README.md](skeleton/README.md) pentru arborele complet (`include/`, `src/`, `build/`, `bin/`, `config/`, `data/`).

## Documentatie Milestone 2

- [docs/SRS.md](docs/SRS.md) — cerinte actualizate
- [docs/SDD.md](docs/SDD.md) — proiectare preliminara
