
#ifndef PANDOC_CONVERT_H
#define PANDOC_CONVERT_H

#include <stddef.h>

/**
 * Verifica daca pandoc este disponibil in sistem.

 */
int pandoc_is_available(void);

/**
 * Converteste un fisier text in format dorit folosind Pandoc.
 * Apeleaza: pandoc -f <fromFormat> -t <toFormat> -o <outputPath> <inputPath>
 */
int pandoc_convert(const char *inputPath,
                   const char *outputPath,
                   const char *fromFormat,
                   const char *toFormat,
                   int timeoutSec);

int pandoc_format_lookup(int outFmt, const char **pandocFmt, const char **extension);

#endif 