
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tesseract/capi.h>  
#include <mupdf/fitz.h>
#include "pdf_ocr.h"

static TessBaseAPI *tess_api = NULL;

int ocr_init(const char *languages) {
    if (tess_api != NULL) {
        return 0;  
    }
    
    tess_api = TessBaseAPICreate();
    if (tess_api == NULL) {
        fprintf(stderr, "ocr_init: TessBaseAPICreate failed\n");
        return -1;
    }
    
    if (TessBaseAPIInit3(tess_api, NULL, languages ? languages : "eng") != 0) {
        fprintf(stderr, "ocr_init: failed to init Tesseract with lang=%s\n", 
                languages ? languages : "eng");
        TessBaseAPIDelete(tess_api);
        tess_api = NULL;
        return -1;
    }
    
    fprintf(stderr, "ocr_init: Tesseract ready, languages=%s\n", 
            languages ? languages : "eng");
    return 0;
}

void ocr_cleanup(void) {
    if (tess_api) {
        TessBaseAPIEnd(tess_api);
        TessBaseAPIDelete(tess_api);
        tess_api = NULL;
    }
}

char *ocr_extract_page_text(fz_context *ctx, fz_document *doc, 
                            int pageNum, int dpi) {
    if (!tess_api) {
        fprintf(stderr, "ocr_extract_page_text: OCR not initialized\n");
        return NULL;
    }
    if (!ctx || !doc) return NULL;
    
    fz_pixmap *pix = NULL;
    char *result = NULL;
    
    fz_try(ctx) {
        fz_matrix mat = fz_scale(dpi / 72.0f, dpi / 72.0f);
        pix = fz_new_pixmap_from_page_number(ctx, doc, pageNum, 
                                              mat, fz_device_rgb(ctx), 0);
    }
    fz_catch(ctx) {
        fprintf(stderr, "ocr_extract_page_text: render failed for page %d\n", pageNum);
        return NULL;
    }
    
    if (!pix) return NULL;
    
    
    int bpp = fz_pixmap_components(ctx, pix);   
    int w   = fz_pixmap_width(ctx, pix);
    int h   = fz_pixmap_height(ctx, pix);
    int stride = fz_pixmap_stride(ctx, pix);
    unsigned char *samples = fz_pixmap_samples(ctx, pix);
    
    TessBaseAPISetImage(tess_api, samples, w, h, bpp, stride);
    TessBaseAPISetSourceResolution(tess_api, dpi);
    
    
    char *tess_text = TessBaseAPIGetUTF8Text(tess_api);
    if (tess_text) {
        result = strdup(tess_text);
        TessDeleteText(tess_text);
    }
    
    fz_drop_pixmap(ctx, pix);
    return result;
}

char *ocr_extract_all_text(fz_context *ctx, fz_document *doc, int dpi) {
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
    
    for (int i = 0; i < pageCount; i++) {
        char *page_text = ocr_extract_page_text(ctx, doc, i, dpi);
        if (!page_text) continue;
        
        size_t len = strlen(page_text);
        size_t header_len = 32; 
        
        while (total_size + len + header_len >= capacity) {
            capacity *= 2;
            char *tmp = realloc(accumulated, capacity);
            if (!tmp) {
                free(page_text);
                free(accumulated);
                return NULL;
            }
            accumulated = tmp;
        }
        
        total_size += snprintf(accumulated + total_size, capacity - total_size,
                               "\n=== Page %d ===\n%s", i + 1, page_text);
        free(page_text);
    }
    
    return accumulated;
}