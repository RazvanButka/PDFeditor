# PDFeditor -- Server Multi-Protocol cu OCR

Server multi-threaded care expune servicii de procesare PDF prin 4 interfețe paralele: UNIX socket, INET TCP, SOAP și PDF TCP. Include recunoaștere text prin Tesseract integrat cu MuPDF.

## Cerințe sistem:

- Ubuntu(sau Docker/Dev Container)
- GCC + Make

## Instalare dependențe

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
make clean
make serverds
make pdfclient
```

## Rulare

### Terminal 1 — server

```bash
./serverds
```

Trebuie să vezi:
```
ocr_init: Tesseract ready, languages=ron+eng
pdf_main: Listening on port 18083
```

### Terminal 2 — client

```bash
./pdfclient /cale/completa/catre/fisier.pdf
```

foloseste cale absolută. PDF-ul trebuie să existe pe mașina serverului.

## Test rapid

```bash
sudo apt install -y enscript ghostscript

cat > /tmp/test.txt << 'EOF'
Ion Popescu
CNP: 1820123456789
Email: ion@example.com
EOF

enscript /tmp/test.txt -o - | ps2pdf - /tmp/test.pdf
./pdfclient /tmp/test.pdf
```





