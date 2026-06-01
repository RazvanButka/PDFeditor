/**
 * Detecteaza si mascheaza in-place urmatoarele tipuri de date personale:
 *   - CNP (Cod Numeric Personal) - 13 cifre, cu validare check digit
 *   - IBAN romanesc - RO + 22 caractere, cu validare modulo 97
 *   - Numere de card credit - 13-19 cifre, cu validare Luhn
 *   - Adrese email - pattern user@domain.tld
 *   - Numere de telefon romanesti - formate 07XX..., +407XX...
 *   - Adrese IPv4 - cu validare octeti 0-255
 *
 
 */
#ifndef GDPR_REDACT_H
#define GDPR_REDACT_H

#include <stddef.h>


int gdpr_init(void);


void gdpr_cleanup(void);

void gdpr_redact_in_place(char *text);

int gdpr_validate_cnp(const char *cnp, size_t len);

int gdpr_validate_iban(const char *iban, size_t len);


int gdpr_validate_luhn(const char *card, size_t len);


#endif /*GDPR_REDACT_H */