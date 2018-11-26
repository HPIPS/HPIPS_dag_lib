/* cryptography, T13.654-T13.826 $DVS:time$ */

#ifndef XDAG_CRYPT_H
#define XDAG_CRYPT_H

#include <stdint.h>
#include "hash.h"

#if defined (__MACOS__) || defined (__APPLE__) 
//MAC�����Ż�SEC256K1
#define USE_OPTIMIZED_EC 0 // 0 disactivate, 1 activated, 2 test openssl vs secp256k1
#else
#define USE_OPTIMIZED_EC 1 // 0 disactivate, 1 activated, 2 test openssl vs secp256k1
#endif

#ifdef __cplusplus
extern "C" {
#endif
	
// ����ϵͳ�ĳ�ʼ��
extern int dag_crypt_init(int withrandom);

// ����һ���µ�˽Կ�͹�Կ
extern void *dag_create_key(dag_hash_t privkey, dag_hash_t pubkey, uint8_t *pubkey_bit);

//ͨ����֪��˽Կ������Կ�͹�Կ���ڲ���ʾ
extern void *dag_private_to_key(const dag_hash_t privkey, dag_hash_t pubkey, uint8_t *pubkey_bit);

// ͨ����֪�Ĺ�Կ������Կ���ڲ���ʾ
extern void *dag_public_to_key(const dag_hash_t pubkey, uint8_t pubkey_bit);

// ɾ���ڲ�����ʾ
extern void dag_free_key(void *key);

// ǩ���ϣ�����������sign_r��sign_s��
extern int dag_sign(const void *key, const dag_hash_t hash, dag_hash_t sign_r, dag_hash_t sign_s);

// ��֤ǩ����sign_r��sign_s���Ƿ��Ӧ��ɢ��'hash'�����Լ���Կ�İ汾
extern int dag_verify_signature(const void *key, const dag_hash_t hash, const dag_hash_t sign_r, const dag_hash_t sign_s);

extern int dag_verify_signature_optimized_ec(const void *key, const dag_hash_t hash, const dag_hash_t sign_r, const dag_hash_t sign_s);
	
#ifdef __cplusplus
};
#endif

#endif
