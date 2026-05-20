/**
Bǎlean Kevin-Lucian
IR3 2026, subgrupa 2

Header pentru modului OCR
Acest modul integreaza biblioteca Tesseract cu MuPDF pentru a extrage text din documente PDF

*/
#ifndef PDF_OCR_H
#define PDF_OCR_H

#include <mupdf/fitz.h> //MuPDF
#include <stddef.h>     //size_t


//Initializam motorul Tesseract si incarcam modelele de limba
int ocr_init(const char *languages);

//Elibereaza resursele Tesseract
void ocr_cleanup(void);

//Extrage textul dintr-o singura pagina a unui PDF folosind OCR
char *ocr_extract_page_text(fz_context *ctx, fz_document *doc, int pageNum, int dpi);

//Extrage textul din toate paginile unui PDF si concatenam rezultatele
char *ocr_extract_all_text(fz_context *ctx, fz_document *doc, int dpi);

char *ocr_extract_all_text_parallel(const char* pdfPath, int dpi, int nrPages);

int ocr_page_to_png(fz_context *ctx, fz_document *doc, int pageNum, int dpi, const char *out_path);

#endif