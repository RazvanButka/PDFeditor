
#ifndef PANDOC_CONVERT_H
#define PANDOC_CONVERT_H

#include <stddef.h>


int pandoc_is_available(void);

int pandoc_convert(const char *inputPath,const char *outputPath,const char *fromFormat,const char *toFormat,int timeoutSec);

int pandoc_convert_with_watermark(const char *inputPath,const char *outputPath,const char *fromFormat,const char *toFormat,const char *watermarkText,int timeoutSec);


int pandoc_format_lookup(int outFmt, const char **pandocFmt, const char **extension);

#endif 