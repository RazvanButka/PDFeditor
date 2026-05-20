# SRS — PDFeditor (actualizare Milestone 2)

## 1. Introducere

**PDFeditor** este un server multi-protocol pentru procesarea documentelor PDF (OCR, extragere text, conversie, watermark, redactare GDPR), cu client remote INET si client de administrare local UNIX.

Aceasta versiune SRS reflecta starea implementata la **Milestone 2**.

## 2. Actori

| Actor | Descriere |
|-------|-----------|
| Client REMOTE (IN) | Aplicatie C pe TCP — upload, procesare, download |
| Client ADMIN (UX) | Aplicatie ncurses pe UNIX socket — monitorizare si control |
| Server | `serverds` — thread-uri UNIX/INET/SOAP/PDF |

## 3. Cerinte functionale

### 3.1 Client ADMIN (UX)

| ID | Cerinta | Prioritate | Status M2 |
|----|---------|------------|-----------|
| UX-01 | Listare clienti conectati | Obligatoriu | Implementat |
| UX-02 | Listare joburi active | Obligatoriu | Implementat |
| UX-03 | Statistici sistem (uptime, clienti, joburi) | Obligatoriu | Implementat |
| UX-04 | Timp mediu executie joburi | Obligatoriu | Implementat |
| UX-05 | Istoric joburi finalizate | Obligatoriu | Implementat |
| UX-06 | Deconectare client (kick) | Obligatoriu | Implementat |
| UX-07 | Oprire fortata job (kill) | Obligatoriu | Implementat |
| UX-08 | Blocare / deblocare IP | Obligatoriu | Implementat |
| UX-09 | Un singur admin conectat | Obligatoriu | Implementat |
| UX-10 | Timeout inactivitate admin | Obligatoriu | Implementat |
| UX-11 | Fara transfer fisiere | Obligatoriu | Implementat |

### 3.2 Client REMOTE (IN)

| ID | Cerinta | Prioritate | Status M2 |
|----|---------|------------|-----------|
| IN-01 | Conectare TCP la server (port 18083) | Obligatoriu | Implementat |
| IN-02 | Upload fisier PDF (chunked) | Obligatoriu | Implementat |
| IN-03 | Download rezultat (chunked) | Obligatoriu | Implementat |
| IN-04 | Interogare status job | Obligatoriu | Implementat |
| IN-05 | OCR document | Obligatoriu | Implementat |
| IN-06 | Extragere text nativ | Obligatoriu | Implementat |
| IN-07 | Conversie format (txt/html/md/rtf/docx la download) | Obligatoriu | Implementat |
| IN-08 | Watermark (banner text + parametru) | Obligatoriu | Implementat |
| IN-09 | Blur / redactare GDPR (text) | Obligatoriu | Implementat |
| IN-10 | Lista joburi client | Optional | Implementat |
| IN-11 | Corelare clientID / jobID | Obligatoriu | Implementat |

**Procent implementat (operatii IN din SRS):** 10/11 ≈ **91%**

### 3.3 Server

| ID | Cerinta | Prioritate | Status M2 |
|----|---------|------------|-----------|
| SV-01 | Procesare joburi in coada FIFO | Recomandat | Implementat |
| SV-02 | Worker dedicat pentru coada | Recomandat | Implementat |
| SV-03 | Procesare paralela prin fork | Obligatoriu | Implementat |
| SV-04 | Stocare input/output (`data/input`, `data/output`) | Obligatoriu | Implementat |
| SV-05 | Configurare libconfig | Optional | Implementat |

## 4. Cerinte non-functionale

- Fisiere de test: pana la **cateva sute MB** (Milestone 2)
- Limbaj server: C, Linux/UNIX
- Dependente: MuPDF, Tesseract, gSOAP, libconfig, pthread

## 5. Protocol

Vezi [`skeleton/include/proto.h`](../skeleton/include/proto.h) — opcodes 10–31 (transfer), 20–25 (PDF), 100–108 (admin).

## 6. Traceabilitate Milestone 2

| Cerinta milestone | Mapare SRS |
|-------------------|------------|
| Admin finalizat, fara fisiere | UX-01 … UX-11 |
| Remote ≥70% operatii | IN-01 … IN-10 |
| Transfer bidirectional | IN-02, IN-03 |
| Coada procesare (plus) | SV-01, SV-02 |
| Progres vs M1 | Structura proiect + server functional |

## 7. Revizii

| Versiune | Data | Descriere |
|----------|------|-----------|
| 0.1 | M1 | Cerinte initiale |
| 0.2 | M2 | Aliniere cu implementare: admin history, coada FIFO, operatii PDF |
