/* cryptography, T13.654-T13.826 $DVS:time$ */

#ifndef XDAG_CRYPT_H
#define XDAG_CRYPT_H

#include <stdint.h>
#include "hash.h"

#if defined (__MACOS__) || defined (__APPLE__) 
//MAC无需优化SEC256K1
#define USE_OPTIMIZED_EC 0 // 0 disactivate, 1 activated, 2 test openssl vs secp256k1
#else
#define USE_OPTIMIZED_EC 1 // 0 disactivate, 1 activated, 2 test openssl vs secp256k1
#endif

#ifdef __cplusplus
extern "C" {
#endif
	
// 加密系统的初始化
extern int dag_crypt_init(int withrandom);

// 创建一对新的私钥和公钥
extern void *dag_create_key(dag_hash_t privkey, dag_hash_t pubkey, uint8_t *pubkey_bit);

//通过已知的私钥返回密钥和公钥的内部表示
extern void *dag_private_to_key(const dag_hash_t privkey, dag_hash_t pubkey, uint8_t *pubkey_bit);

// 通过已知的公钥返回密钥的内部表示
extern void *dag_public_to_key(const dag_hash_t pubkey, uint8_t pubkey_bit);

// 删除内部键表示
extern void dag_free_key(void *key);

// 签署哈希并将结果放在sign_r和sign_s中
extern int dag_sign(const void *key, const dag_hash_t hash, dag_hash_t sign_r, dag_hash_t sign_s);

// 验证签名（sign_r，sign_s）是否对应于散列'hash'，即自己密钥的版本
extern int dag_verify_signature(const void *key, const dag_hash_t hash, const dag_hash_t sign_r, const dag_hash_t sign_s);

extern int dag_verify_signature_optimized_ec(const void *key, const dag_hash_t hash, const dag_hash_t sign_r, const dag_hash_t sign_s);
	
#ifdef __cplusplus
};
#endif

#endif
