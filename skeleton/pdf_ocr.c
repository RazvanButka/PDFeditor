/**
Programul de mai jos implementeaza recunoasterea caracterelor pentru documentele PDF cu ajutorul integrarii:
* MuPDF care randeaza paginile unui PDF in imagini
* Tesseract OCR care extrage textul din imaginile create
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tesseract/capi.h>  
#include <mupdf/fitz.h>
#include "pdf_ocr.h"

static TessBaseAPI *tess_api = NULL;

int ocr_init(const char *languages) { 
    if (tess_api != NULL) { //
        return 0;  
    }
    
    tess_api = TessBaseAPICreate();
    if (tess_api == NULL) {
        fprintf(stderr, "ocr_init: TessBaseAPICreate failed\n");
        return -1;
    }
    
    if (TessBaseAPIInit3(tess_api, NULL, languages ? languages : "eng") != 0) {
        fprintf(stderr, "ocr_init: failed to init Tesseract with lang=%s\n",languages ? languages : "eng");
        TessBaseAPIDelete(tess_api);
        tess_api = NULL;
        return -1;
    }
    
    fprintf(stderr, "ocr_init: Tesseract ready, languages=%s\n", languages ? languages : "eng");
    return 0;
}

void ocr_cleanup(void) { //Functia de cleanup
    if (tess_api) {
        TessBaseAPIEnd(tess_api); //elibereaza modelele neurale
        TessBaseAPIDelete(tess_api); //elibereaza structura 
        tess_api = NULL;
    }
}

char *ocr_extract_page_text(fz_context *ctx, fz_document *doc,int pageNum, int dpi) { //Aici se transforma pagina PDF -> imagine si in text
    if (!tess_api) { 
        fprintf(stderr, "ocr_extract_page_text: OCR not initialized\n");
        return NULL;
    }
    if (!ctx || !doc) return NULL;
    
    fz_pixmap *pix = NULL;
    char *result = NULL;
    
    fz_try(ctx) {
        fz_matrix mat = fz_scale(dpi / 72.0f, dpi / 72.0f); //Am creat o matrice de tranformare 2D de scalare uniforma
        pix = fz_new_pixmap_from_page_number(ctx, doc, pageNum,mat, fz_device_rgb(ctx), 0);   //Returnam pixmap-ul 
    }
    fz_catch(ctx) { //Partea de eroare
        fprintf(stderr, "ocr_extract_page_text: render failed for page %d\n", pageNum);
        return NULL;
    }
    
    if (!pix) return NULL;
    
    
    int bpp = fz_pixmap_components(ctx, pix);   //nr de canale de culoare
    int w = fz_pixmap_width(ctx, pix); 
    int h= fz_pixmap_height(ctx, pix);
    int stride = fz_pixmap_stride(ctx, pix); //nr de bytes dintr-o linie a imaginii
    unsigned char *samples = fz_pixmap_samples(ctx, pix); //buffer-ul 
    
    TessBaseAPISetImage(tess_api, samples, w, h, bpp, stride); //Memoram pointerul samples
    TessBaseAPISetSourceResolution(tess_api, dpi); // Spunem tesseract-ului ca imaginea este capturata la 300DPI
    
    
    char *tess_text = TessBaseAPIGetUTF8Text(tess_api); //analizam layout ul paginii si combinam caracterele in text UTF-8
    if (tess_text) {
        result = strdup(tess_text);
        TessDeleteText(tess_text);
    }
    
    fz_drop_pixmap(ctx, pix);
    return result;
}

char *ocr_extract_all_text(fz_context *ctx, fz_document *doc, int dpi) { //Facem OCR-ul pentru toate paginile unui PDF
    if (!ctx || !doc) return NULL;
    
    int pageCount = 0;
    fz_try(ctx) {
        pageCount = fz_count_pages(ctx, doc);
    }
    fz_catch(ctx) {
        return NULL;
    }
    
    size_t total_size = 0;
    size_t capacity = 4096;
    char *accumulated = malloc(capacity);
    if (!accumulated) return NULL;
    accumulated[0] = '\0';
    
    for (int i = 0; i < pageCount; i++) { //procesam pagina cu pagina
        char *page_text = ocr_extract_page_text(ctx, doc, i, dpi);
        if (!page_text) continue;
        
        size_t len = strlen(page_text);
        size_t header_len = 32; 
        
        while (total_size + len + header_len >= capacity) { //aici dublam buffer-ul in cazul in care avem nevoie de mai mult spatiu
            capacity *= 2;
            char *tmp = realloc(accumulated, capacity);
            if (!tmp) {
                free(page_text);
                free(accumulated);
                return NULL;
            }
            accumulated = tmp;
        }
        
        total_size += snprintf(accumulated + total_size, capacity - total_size,"\n=== Page %d ===\n%s", i + 1, page_text);
        free(page_text);
    }
    
    return accumulated;
}