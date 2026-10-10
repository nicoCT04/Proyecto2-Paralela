/**
 * @file    des_utils.h
 * @brief   Rutinas DES compartidas por todos los programas del proyecto.
 *
 * Reemplazan a las de `rpc/des_crypt.h` del bruteforce.c original, que ya no
 * existe en las distribuciones modernas de Linux. Usan la API DES de OpenSSL
 * (<openssl/des.h>), enlazada con -lcrypto.
 *
 * Equivalencias con bruteforce.c:
 *   encrypt(key, ciph, len)  ->  encrypt_des(key, buffer, length)
 *   decrypt(key, ciph, len)  ->  decrypt_des(key, buffer, length)
 *   tryKey(key, ciph, len)   ->  try_key(key, cipher, length, phrase, workspace)
 */
#ifndef DES_UTILS_H
#define DES_UTILS_H

#include <stddef.h>
#include <stdint.h>

/** Tamaño de un bloque DES en bytes (64 bits). */
#define DES_BLOCK_BYTES 8

/** Bits efectivos de una llave DES: el espacio de búsqueda completo es 2^56. */
#define DES_KEY_BITS 56

/**
 * @brief  Convierte una llave de 56 bits en los 8 bytes que espera DES.
 * @param  key56  Llave numérica (solo se usan sus 56 bits bajos).
 * @param  out    Salida: 8 bytes; cada byte lleva 7 bits de la llave y 1 de paridad.
 */
void build_des_key(uint64_t key56, unsigned char out[DES_BLOCK_BYTES]);

/**
 * @brief  Cifra `buffer` in-place con DES en modo ECB.
 * @param  key     Llave de 56 bits.
 * @param  buffer  Datos a cifrar; se sobrescriben con el resultado.
 * @param  length  Bytes de `buffer`; debe ser múltiplo de DES_BLOCK_BYTES.
 */
void encrypt_des(uint64_t key, unsigned char *buffer, size_t length);

/**
 * @brief  Descifra `buffer` in-place con DES en modo ECB (inversa de encrypt_des).
 * @param  key     Llave de 56 bits.
 * @param  buffer  Datos cifrados; se sobrescriben con el texto plano.
 * @param  length  Bytes de `buffer`; debe ser múltiplo de DES_BLOCK_BYTES.
 */
void decrypt_des(uint64_t key, unsigned char *buffer, size_t length);

/**
 * @brief  Prueba una llave candidata: descifra una copia y busca la frase clave.
 * @param  key        Llave candidata.
 * @param  cipher     Texto cifrado original (no se modifica).
 * @param  length     Bytes de `cipher` (múltiplo de DES_BLOCK_BYTES).
 * @param  phrase     Frase clave conocida de antemano.
 * @param  workspace  Buffer de trabajo de al menos `length + 1` bytes, reservado
 *                    una sola vez por el llamador para no pedir memoria en cada intento.
 * @return 1 si el texto descifrado contiene `phrase`, 0 si no.
 */
int try_key(uint64_t key, const unsigned char *cipher, size_t length,
            const char *phrase, unsigned char *workspace);

/**
 * @brief  Redondea una longitud al siguiente múltiplo de DES_BLOCK_BYTES.
 * @param  length  Longitud original en bytes.
 * @return Longitud con relleno (padding) incluido.
 */
size_t padded_length(size_t length);

#endif /* DES_UTILS_H */
