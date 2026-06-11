/**
Bǎlean Kevin-Lucian, Donea Andrei
IR3 2026, subgrupa 2
Programul de mai jos implementeaza recunoasterea caracterelor pentru documentele PDF cu ajutorul integrarii:
* MuPDF care randeaza paginile unui PDF in imagini
* Tesseract OCR care extrage textul din imaginile create
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <tesseract/capi.h>  
#include <stdlib.h>
#include <mupdf/fitz.h>
#include "pdf_ocr.h"


#define MAX_CONCURRENT_OCR 4

static TessBaseAPI *tess_api = NULL;

typedef struct {
    int pipeReadFileDescriptor;  /* capat citire al pipe-ului (parinte) */
    pid_t childProcessID;   /* PID-ul procesului copil */
    int pageIndex;    /* indice pagina pentru identificare */
} pagePipeInfo;

int ocr_init(const char *languages) { 
    if (tess_api != NULL) { //
        return 0;  
    }
    
    tess_api = TessBaseAPICreate();
    if (tess_api == NULL) {
        (void)fprintf(stderr, "ocr_init: TessBaseAPICreate failed\n");
        return -1;
    }

    //const char *lang = (languages != NULL) ? languages : "eng";
    
    if (TessBaseAPIInit3(tess_api, NULL, languages ? languages : "eng") != 0) {
        (void)fprintf(stderr, "ocr_init: failed to init Tesseract with lang=%s\n",languages ? languages : "eng");
        TessBaseAPIDelete(tess_api);
        tess_api = NULL;
        return -1;
    }
    
    (void)fprintf(stderr, "ocr_init: Tesseract ready, languages=%s\n", languages ? languages : "eng");
    return 0;
}

void ocr_cleanup(void) { //Functia de cleanup
    if(tess_api == NULL) {
        return; 
    }
    if (tess_api) {
        TessBaseAPIEnd(tess_api); //elibereaza modelele neurale
        TessBaseAPIDelete(tess_api); //elibereaza structura 
        tess_api = NULL;
    }
}

static fz_pixmap *render_page_pixmap(fz_context* context, fz_document* document, int pageNumber, int dpi){
    fz_matrix mat = fz_scale((float)dpi / 72.0f, (float)dpi / 72.0f); //Matrice de scalare uniforma
    fz_pixmap *pixmap = NULL;
    
    fz_try(context) {
        pixmap = fz_new_pixmap_from_page_number(context, document, pageNumber, mat, fz_device_rgb(context), 0);
    } 
    fz_catch(context) {
        (void)fprintf(stderr, "render_page_pixmap: failed to render page %d\n", pageNumber);
        pixmap = NULL;
    }
    
    return pixmap;
}

char *ocr_extract_page_text(fz_context *context, fz_document *document, int pageNum, int dpi) { //Aici se transforma pagina PDF -> imagine si in text
    if (tess_api == NULL) { 
        (void)fprintf(stderr, "ocr_extract_page_text: OCR not initialized\n");
        return NULL;
    }
    if (context == NULL || document == NULL) return NULL;
    
    fz_pixmap *pixmap = render_page_pixmap(context, document, pageNum, dpi);
    if (pixmap == NULL) return NULL;

    int bpp = fz_pixmap_components(context, pixmap);
    int w = fz_pixmap_width(context, pixmap);
    int h = fz_pixmap_height(context, pixmap);
    int stride = fz_pixmap_stride(context, pixmap);
    unsigned char *samples = fz_pixmap_samples(context, pixmap);

    TessBaseAPISetImage(tess_api, samples, w, h, bpp, stride);
    TessBaseAPISetSourceResolution(tess_api, dpi);

    char* tesseractText = TessBaseAPIGetUTF8Text(tess_api);
    char* result = NULL;
    if (tesseractText != NULL) {
        result = strdup(tesseractText);
        TessDeleteText(tesseractText);
    }
    
    fz_drop_pixmap(context, pixmap);
    return result;
}

char *ocr_extract_all_text(fz_context *context, fz_document *document, int dpi) { //Facem OCR-ul pentru toate paginile unui PDF
    if (!context || !document) return NULL;
    
    int pageCount = 0;
    fz_try(context) {
        pageCount = fz_count_pages(context, document);
    }
    fz_catch(context) {
        (void)fprintf(stderr, "ocr_extract_all_text: fz_count_pages failed\n");
        return NULL;
    }
    
    size_t totalSize = 0;
    size_t capacity = 4096;
    char *accumulated = malloc(capacity);
    if (!accumulated){
        (void)fprintf(stderr, "ocr_extract_all_text: malloc failed\n");
        return NULL;
    } 
    accumulated[0] = '\0';
    
    for (int i = 0; i < pageCount; i++) { //procesam pagina cu pagina
        char *pageText = ocr_extract_page_text(context, document, i, dpi);
        if (!pageText) continue;
        
        size_t textLength = strlen(pageText);
        size_t needed = totalSize + textLength + 64; //64 bytes pentru antetul paginii
        
        while (needed >= capacity) { //aici dublam buffer-ul in cazul in care avem nevoie de mai mult spatiu
            capacity *= 2;
            char *tmp = realloc(accumulated, capacity);
            if (!tmp) {
                free(pageText);
                free(accumulated);
                return NULL;
            }
            accumulated = tmp;
        }
        
        int written= snprintf(accumulated + totalSize, capacity - totalSize,"\n=== Page %d ===\n%s", i + 1, pageText);
        if(written > 0) {
            totalSize += (size_t)written;
        }
        free(pageText);
    }
    return accumulated;
}

char* ocr_extract_all_text_parallel(const char* pdfPath, int dpi, int nrPages) {
    /* Varianta paralela cu limitare a concurentei.
     * Foloseste batch processing: lanseaza maxim MAX_CONCURRENT_OCR procese
     * concomitent, asteapta sa termine, apoi trece la urmatorul batch.
     * Asigura consum RAM limitat indiferent de numarul de pagini. */

    if (pdfPath == NULL || pdfPath[0] == '\0' || nrPages <= 0) {
        return NULL;
    }

    pagePipeInfo *pipes = malloc((size_t)nrPages * sizeof(pagePipeInfo));
    if(pipes == NULL) {
        (void)fprintf(stderr, "ocr_extract_all_text_parallel: malloc pipes failed\n");
        return NULL;
    }

    /* Initializare: toate intrarile sunt invalide pana cand fork-ul reuseste */
    for (int i = 0; i < nrPages; i++) {
        pipes[i].pipeReadFileDescriptor = -1;
        pipes[i].childProcessID = -1;
        pipes[i].pageIndex = i;
    }

    size_t capacity = 8192;
    size_t total_size = 0;
    char *result = malloc(capacity);
    if (result == NULL) {
        (void)fprintf(stderr, "ocr_extract_all_text_parallel: malloc result failed\n");
        free(pipes);
        return NULL;
    }
    result[0] = '\0';

    /* Procesare in batch-uri de MAX_CONCURRENT_OCR pagini */
    for (int batchStart = 0; batchStart < nrPages; batchStart += MAX_CONCURRENT_OCR) {
        int batchEnd = batchStart + MAX_CONCURRENT_OCR;
        if (batchEnd > nrPages) batchEnd = nrPages;

        (void)fprintf(stderr, "[ocr-parallel] Batch pagini %d-%d (din %d total)\n",
                      batchStart + 1, batchEnd, nrPages);

        /* === Faza 1: Lanseaza fork-urile pentru paginile din batch === */
        for (int i = batchStart; i < batchEnd; i++) {
            int pfd[2];
            if (pipe(pfd) < 0) {
                (void)fprintf(stderr, "ocr_extract_all_text_parallel: pipe() failed page %d: %s\n",
                              i, strerror(errno));
                continue;
            }

            pid_t pid = fork();
            if (pid < 0) {
                (void)fprintf(stderr, "ocr_extract_all_text_parallel: fork() failed page %d: %s\n",
                              i, strerror(errno));
                close(pfd[0]);
                close(pfd[1]);
                continue;
            }

            if (pid == 0) { /* Proces copil */
                close(pfd[0]); /* copilul nu citeste */

                fz_context *childContext = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
                fz_document *childDocument = NULL;
                char *text = NULL;

                if(childContext != NULL){
                    fz_try(childContext){
                        fz_register_document_handlers(childContext);
                    } fz_catch(childContext){
                        fz_drop_context(childContext);
                        childContext = NULL;
                    }
                }
                if(childContext != NULL){
                    fz_try(childContext){
                        childDocument = fz_open_document(childContext, pdfPath);
                    } fz_catch(childContext){
                        childDocument = NULL;
                    }
                }
                if(childContext != NULL && childDocument != NULL){
                     tess_api = NULL;
                     if(ocr_init("ron+eng") == 0){
                        text = ocr_extract_page_text(childContext, childDocument, i, dpi);
                        ocr_cleanup();
                     }
                 }
                if(childDocument != NULL){
                    fz_drop_document(childContext, childDocument);
                }
                if(childContext != NULL){
                    fz_drop_context(childContext);
                }

                 /* Scriem lungimea + textul in pipe. */
                 if (text != NULL) {
                    size_t len = strlen(text);
                    if (write(pfd[1], &len, sizeof(size_t)) < 0 || write(pfd[1], text, len) < 0) 
                        (void)fprintf(stderr, "copil pagina %d: write pipe failed\n", i);
                    free(text);
                 } else {
                    size_t zero = 0;
                    (void)write(pfd[1], &zero, sizeof(size_t));
                 }
                 (void)close(pfd[1]);
                 _exit(EXIT_SUCCESS);
            }
            (void)close(pfd[1]); /* parinte nu scrie */
            pipes[i].pipeReadFileDescriptor = pfd[0];
            pipes[i].childProcessID = pid;
        }

        /* === Faza 2: Colecteaza rezultatele copiilor din batch (blocheaza pana toti termina) === */
        for (int i = batchStart; i < batchEnd; i++){
            char *page_text = NULL;
            size_t page_len = 0;

            if (pipes[i].pipeReadFileDescriptor >= 0){
                ssize_t r = read(pipes[i].pipeReadFileDescriptor, &page_len, sizeof(size_t));
                if(r == (ssize_t)sizeof(size_t) && page_len > 0){
                    page_text = malloc(page_len + 1);
                    if (page_text != NULL){
                        ssize_t total_read = 0;
                        while ((size_t)total_read < page_len){
                            ssize_t n = read(pipes[i].pipeReadFileDescriptor, page_text + total_read,
                                             page_len - (size_t)total_read);
                            if (n <= 0)
                                break;
                            total_read += n;
                        }
                        page_text[page_len] = '\0';
                    }
                }
                (void)close(pipes[i].pipeReadFileDescriptor);
            }

            if (pipes[i].childProcessID > 0){
                int wstatus;
                (void)waitpid(pipes[i].childProcessID, &wstatus, 0);
            }

            const char *content = (page_text != NULL) ? page_text : "[OCR esuat pentru aceasta pagina]";

            size_t content_len = strlen(content);
            size_t needed = total_size + content_len + 64;

            while (needed >= capacity)
            {
                capacity *= 2;
                char *tmp = realloc(result, capacity);
                if (tmp == NULL){
                    (void)fprintf(stderr, "ocr_extract_all_text_parallel: realloc failed\n");
                    free(page_text);
                    free(result);
                    free(pipes);
                    return NULL;
                }
                result = tmp;
            }

            int written = snprintf(result + total_size, capacity - total_size, "\n=== Page %d ===\n%s",
                                   i + 1, content);
            if (written > 0)
                total_size += (size_t)written;

            free(page_text);
        }
        /* Toti copiii batch-ului au fost colectati. Putem trece la urmatorul. */
    }

    free(pipes);
    return result;
}

int ocr_page_to_png(fz_context *context, fz_document *document, int pageNum, int dpi, const char *outputPath) {
    if (context == NULL || document == NULL || outputPath == NULL) {
        return -1;
    }

    fz_pixmap *pixmap = render_page_pixmap(context, document, pageNum, dpi);
    if (pixmap == NULL) {
        return -1;
    }

    int rc = 0;

    fz_try(context) {
        fz_save_pixmap_as_png(context, pixmap, outputPath);
    } fz_catch(context) {
        (void)fprintf(stderr, "ocr_page_to_png: failed to write PNG for page %d\n", pageNum);
        fz_drop_pixmap(context, pixmap);
        rc = -1;
    }

    fz_drop_pixmap(context, pixmap);
    return rc;
}
