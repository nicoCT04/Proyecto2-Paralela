/**
 * @file    des_utils.c
 * @brief   Implementación de las rutinas DES sobre OpenSSL (ver des_utils.h).
 */
#include "des_utils.h"

#include <openssl/des.h>
#include <string.h>

void build_des_key(uint64_t key56, unsigned char out[DES_BLOCK_BYTES])
{
    /*
     * DES recibe 64 bits, pero el bit menos significativo de cada byte es de
     * paridad. Igual que el decrypt() de bruteforce.c, repartimos los 56 bits
     * de la llave en grupos de 7: el byte i guarda los bits 7i..7i+6 en sus
     * posiciones 1..7, y DES_set_odd_parity rellena la posición 0.
     */
    for (int i = 0; i < DES_BLOCK_BYTES; ++i) {
        out[i] = (unsigned char)(((key56 >> (7 * i)) & 0x7F) << 1);
    }
    DES_set_odd_parity((DES_cblock *)out);
}

/**
 * @brief  Aplica DES bloque a bloque sobre `buffer` en la dirección indicada.
 * @param  key        Llave de 56 bits.
 * @param  buffer     Datos (in-place).
 * @param  length     Bytes de `buffer`, múltiplo de DES_BLOCK_BYTES.
 * @param  direction  DES_ENCRYPT o DES_DECRYPT.
 */
static void run_des(uint64_t key, unsigned char *buffer, size_t length, int direction)
{
    DES_cblock key_block;
    DES_key_schedule schedule;

    build_des_key(key, key_block);
    /* "unchecked": no rechaza llaves débiles; en fuerza bruta hay que probarlas todas. */
    DES_set_key_unchecked(&key_block, &schedule);

    for (size_t offset = 0; offset + DES_BLOCK_BYTES <= length; offset += DES_BLOCK_BYTES) {
        DES_cblock *block = (DES_cblock *)(buffer + offset);
        DES_ecb_encrypt((const_DES_cblock *)block, block, &schedule, direction);
    }
}

void encrypt_des(uint64_t key, unsigned char *buffer, size_t length)
{
    run_des(key, buffer, length, DES_ENCRYPT);
}

void decrypt_des(uint64_t key, unsigned char *buffer, size_t length)
{
    run_des(key, buffer, length, DES_DECRYPT);
}

int try_key(uint64_t key, const unsigned char *cipher, size_t length,
            const char *phrase, unsigned char *workspace)
{
    /* Se descifra una copia (memcpy) para no destruir el cifrado original. */
    memcpy(workspace, cipher, length);
    workspace[length] = '\0';
    decrypt_des(key, workspace, length);
    /* Con la llave correcta, el texto plano contiene la frase clave. */
    return strstr((const char *)workspace, phrase) != NULL;
}

size_t padded_length(size_t length)
{
    return (length + DES_BLOCK_BYTES - 1) / DES_BLOCK_BYTES * DES_BLOCK_BYTES;
}
