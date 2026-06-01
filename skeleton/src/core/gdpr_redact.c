
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdpr_redact.h"


static pcre2_code *gReCNP   = NULL;  /* 13 cifre consecutive */
static pcre2_code *gReIBAN  = NULL;  /* RO + 2 cifre + 4 litere + 16 cifre */
static pcre2_code *gReCard  = NULL;  /* 13-19 cifre (cu separatori optional) */
static pcre2_code *gReEmail = NULL;  /* pattern email standard */
static pcre2_code *gRePhone = NULL;  /* telefon romanesc */
static pcre2_code *gReIP    = NULL;  /* IPv4 */

static pcre2_code *compileRegex(const char *pattern, const char *name)
{
    int errcode;
    PCRE2_SIZE erroffset;
    pcre2_code *re = pcre2_compile(
        (PCRE2_SPTR)pattern,
        PCRE2_ZERO_TERMINATED,
        0,
        &errcode,
        &erroffset,
        NULL
    );
    if (re == NULL) {
        PCRE2_UCHAR errbuf[256];
        pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));
        (void)fprintf(stderr, "gdpr_init: compilare regex '%s' esuata la offset %zu: %s\n",
                      name, (size_t)erroffset, errbuf);
    }
    return re;
}



int gdpr_validate_cnp(const char *cnp, size_t len)
{
    if (cnp == NULL) return 0;
    if (len != 13) return 0;

    /* Prima cifra trebuie 1-9 (gen + secol):
     *   1,2 = sec XX (1900-1999) | 3,4 = sec XIX (1800-1899)
     *   5,6 = sec XXI (2000-2099) | 7,8 = rezidenti straini | 9 = strain */
    if (cnp[0] < '1' || cnp[0] > '9') return 0;

    for (size_t i = 0; i < 13; i++) {
        if (cnp[i] < '0' || cnp[i] > '9') return 0;
    }

    /* Ponderi oficiale CNP, sursa: standardul roman */
    static const int weights[12] = {2, 7, 9, 1, 4, 6, 3, 5, 8, 2, 7, 9};
    int sum = 0;
    for (int i = 0; i < 12; i++) {
        sum += (cnp[i] - '0') * weights[i];
    }
    int checkDigit = sum % 11;
    if (checkDigit == 10) checkDigit = 1;

    return (cnp[12] - '0') == checkDigit;
}

int gdpr_validate_luhn(const char *card, size_t len)
{
    if (card == NULL || len < 13 || len > 19) return 0;

    int sum = 0;
    int parity = (int)(len % 2);

    for (size_t i = 0; i < len; i++) {
        if (card[i] < '0' || card[i] > '9') return 0;
        int digit = card[i] - '0';
        if ((int)(i % 2) == parity) {
            digit *= 2;
            if (digit > 9) digit -= 9;
        }
        sum += digit;
    }
    return (sum % 10) == 0;
}

int gdpr_validate_iban(const char *iban, size_t len)
{
    if (iban == NULL) return 0;
    if (len < 15 || len > 34) return 0;

    char numeric[100];
    size_t pos = 0;

    for (size_t i = 4; i < len && pos < sizeof(numeric) - 3; i++) {
        char c = iban[i];
        if (c >= '0' && c <= '9') {
            numeric[pos++] = c;
        } else if (c >= 'A' && c <= 'Z') {
            int val = c - 'A' + 10;
            numeric[pos++] = '0' + (val / 10);
            numeric[pos++] = '0' + (val % 10);
        } else {
            return 0; 
        }
    }
    for (size_t i = 0; i < 4 && pos < sizeof(numeric) - 3; i++) {
        char c = iban[i];
        if (c >= '0' && c <= '9') {
            numeric[pos++] = c;
        } else if (c >= 'A' && c <= 'Z') {
            int val = c - 'A' + 10;
            numeric[pos++] = '0' + (val / 10);
            numeric[pos++] = '0' + (val % 10);
        } else {
            return 0;
        }
    }

    /* Calculam (numeric mod 97) cu metoda Horner (numarul e prea mare
     * pentru uint64_t pentru IBAN-uri mai lungi).
     * Daca rezultatul e 1, IBAN-ul e valid. */
    int remainder = 0;
    for (size_t i = 0; i < pos; i++) {
        remainder = (remainder * 10 + (numeric[i] - '0')) % 97;
    }
    return remainder == 1;
}

/** Valideaza ca un IPv4 are toti octeti in [0, 255]. */
static int validateIPv4(const char *ip, size_t len)
{
    if (ip == NULL || len < 7 || len > 15) return 0;

    int octets = 0;
    int currentOctet = 0;
    int digitsInOctet = 0;

    for (size_t i = 0; i < len; i++) {
        char c = ip[i];
        if (c == '.') {
            if (digitsInOctet == 0) return 0;
            octets++;
            currentOctet = 0;
            digitsInOctet = 0;
        } else if (c >= '0' && c <= '9') {
            currentOctet = currentOctet * 10 + (c - '0');
            digitsInOctet++;
            if (currentOctet > 255 || digitsInOctet > 3) return 0;
        } else {
            return 0;
        }
    }
    return octets == 3 && digitsInOctet > 0;
}



int gdpr_init(void)
{
    
    gReCNP = compileRegex("(?<!\\d)\\d{13}(?!\\d)", "CNP");

    gReIBAN = compileRegex("\\bRO\\d{2}[A-Z]{4}[A-Z0-9]{16}\\b", "IBAN");

    gReCard = compileRegex(
        "(?<!\\d)(?:\\d{4}[ -]?\\d{4}[ -]?\\d{4}[ -]?\\d{1,7}|\\d{13,19})(?!\\d)",
        "Card"
    );

    gReEmail = compileRegex(
        "[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}",
        "Email"
    );

    gRePhone = compileRegex(
        "(?<!\\d)(?:\\+?40\\s?7\\d{8}|07\\d{2}[ .-]?\\d{3}[ .-]?\\d{3}|0[23]\\d{2}[ .-]?\\d{3}[ .-]?\\d{3})(?!\\d)",
        "Phone"
    );


    gReIP = compileRegex(
        "(?<!\\d)\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}(?!\\d)",
        "IP"
    );

    if (gReCNP == NULL || gReIBAN == NULL || gReCard == NULL ||
        gReEmail == NULL || gRePhone == NULL || gReIP == NULL) {
        gdpr_cleanup();
        return -1;
    }
    return 0;
}

void gdpr_cleanup(void)
{
    if (gReCNP)   { pcre2_code_free(gReCNP);   gReCNP   = NULL; }
    if (gReIBAN)  { pcre2_code_free(gReIBAN);  gReIBAN  = NULL; }
    if (gReCard)  { pcre2_code_free(gReCard);  gReCard  = NULL; }
    if (gReEmail) { pcre2_code_free(gReEmail); gReEmail = NULL; }
    if (gRePhone) { pcre2_code_free(gRePhone); gRePhone = NULL; }
    if (gReIP)    { pcre2_code_free(gReIP);    gReIP    = NULL; }
}



typedef int (*Validator)(const char *match, size_t len);


static int validateCNPWrapper(const char *m, size_t len) {
    return gdpr_validate_cnp(m, len);
}
static int validateIBANWrapper(const char *m, size_t len) {
    return gdpr_validate_iban(m, len);
}
static int validateCardWrapper(const char *m, size_t len) {
    char digits[20];
    size_t pos = 0;
    for (size_t i = 0; i < len && pos < sizeof(digits); i++) {
        if (m[i] >= '0' && m[i] <= '9') digits[pos++] = m[i];
    }
    if (pos < 13 || pos > 19) return 0;
    return gdpr_validate_luhn(digits, pos);
}
static int validateIPWrapper(const char *m, size_t len) {
    return validateIPv4(m, len);
}
static int validateAlwaysTrue(const char *m, size_t len) {
    (void)m; (void)len;
    return 1;  
}


static void redactWithRegex(char *text, pcre2_code *re, Validator validate)
{
    if (text == NULL || re == NULL) return;

    pcre2_match_data *matchData = pcre2_match_data_create_from_pattern(re, NULL);
    if (matchData == NULL) return;

    size_t textLen = strlen(text);
    PCRE2_SIZE startOffset = 0;

    while (startOffset < textLen) {
        int rc = pcre2_match(
            re,
            (PCRE2_SPTR)text,
            textLen,
            startOffset,
            0,
            matchData,
            NULL
        );

        if (rc < 0) break; 

        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(matchData);
        PCRE2_SIZE matchStart = ovector[0];
        PCRE2_SIZE matchEnd = ovector[1];
        size_t matchLen = matchEnd - matchStart;

       
        int isValid = 1;
        if (validate != NULL) {
            isValid = validate(text + matchStart, matchLen);
        }

        if (isValid) {
            /* Mascheaza toate caracterele matchului cu '*'
             * Pastram doar caracterele non-alfanumerice (ex: '@', '.', '-')
             * pentru ca structura datei sa ramana vizibila la prima vedere. */
            for (PCRE2_SIZE i = matchStart; i < matchEnd; i++) {
                text[i] = '*';
            }
        }

        startOffset = (matchEnd > matchStart) ? matchEnd : matchStart + 1;
    }

    pcre2_match_data_free(matchData);
}

void gdpr_redact_in_place(char *text)
{
    if (text == NULL) return;

    

    redactWithRegex(text, gReIBAN,  validateIBANWrapper);   
    redactWithRegex(text, gReCNP,   validateCNPWrapper);    
    redactWithRegex(text, gReCard,  validateCardWrapper);  
    redactWithRegex(text, gRePhone, validateAlwaysTrue);   
    redactWithRegex(text, gReEmail, validateAlwaysTrue);    
    redactWithRegex(text, gReIP,    validateIPWrapper);     
}