#ifndef PDF_OCR_H
#define PDF_OCR_H

#include <mupdf/fitz.h>

int ocr_init(const char *languages);


void ocr_cleanup(void);


char *ocr_extract_page_text(fz_context *ctx, fz_document *doc, int pageNum, int dpi);


char *ocr_extract_all_text(fz_context *ctx, fz_document *doc, int dpi);

#endif