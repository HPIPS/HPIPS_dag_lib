/* cryptography (ECDSA), T13.654-T13.847 $DVS:time$ */

#include <string.h>
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ecdsa.h>
#include "crypt.h"
#include "transport.h"
#include "utils/log.h"
#include "system.h"

#if USE_OPTIMIZED_EC == 1 || USE_OPTIMIZED_EC == 2

#include "../secp256k1/include/secp256k1.h"

secp256k1_context *ctx_noopenssl;

#endif

static EC_GROUP *group;

extern unsigned int xOPENSSL_ia32cap_P[4];
extern int xOPENSSL_ia32_cpuid(unsigned int *);

// 加密系统的初始化
///* 输入随机数 withrandom
int dag_crypt_init(int withrandom)
{
	if(withrandom) {
		uint64_t buf[64];
		xOPENSSL_ia32_cpuid(xOPENSSL_ia32cap_P);
		dag_generate_random_array(buf, sizeof(buf));
		dag_debug("Seed  : [%s]", dag_log_array(buf, sizeof(buf)));
		RAND_seed(buf, sizeof(buf));
	}

#if USE_OPTIMIZED_EC == 1 || USE_OPTIMIZED_EC == 2
	ctx_noopenssl = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
#endif

	group = EC_GROUP_new_by_curve_name(NID_secp256k1);
	if(!group) return -1;

	return 0;
}

/* 创建一对新的私钥和公钥
 * 私钥保存到'privkey'数组，'pubkey'数组的公钥，
 * 公钥的奇偶校验保存到变量'pubkey_bit'
 * 返回指向其内部表示的指针
 */
void *dag_create_key(dag_hash_t privkey, dag_hash_t pubkey, uint8_t *pubkey_bit)
{
	uint8_t buf[sizeof(dag_hash_t) + 1];
	EC_KEY *eckey = 0;
	const BIGNUM *priv = 0;
	const EC_POINT *pub = 0;
	BN_CTX *ctx = 0;
	int res = -1;

	if(!group) {
		goto fail;
	}

	eckey = EC_KEY_new();

	if(!eckey) {
		goto fail;
	}

	if(!EC_KEY_set_group(eckey, group)) {
		goto fail;
	}

	if(!EC_KEY_generate_key(eckey)) {
		goto fail;
	}

	priv = EC_KEY_get0_private_key(eckey);
	if(!priv) {
		goto fail;
	}

	if(BN_bn2bin(priv, (uint8_t*)privkey) != sizeof(dag_hash_t)) {
		goto fail;
	}

	pub = EC_KEY_get0_public_key(eckey);
	if(!pub) {
		goto fail;
	}

	ctx = BN_CTX_new();
	if(!ctx) {
		goto fail;
	}

	BN_CTX_start(ctx);
	if(EC_POINT_point2oct(group, pub, POINT_CONVERSION_COMPRESSED, buf, sizeof(dag_hash_t) + 1, ctx) != sizeof(dag_hash_t) + 1) {
		goto fail;
	}

	memcpy(pubkey, buf + 1, sizeof(dag_hash_t));
	*pubkey_bit = *buf & 1;
	res = 0;

fail:
	if(ctx) {
		BN_CTX_free(ctx);
	}

	if(res && eckey) {
		EC_KEY_free(eckey);
	}

	return res ? 0 : eckey;
}

// 通过已知的私钥返回密钥和公钥的内部表示
void *dag_private_to_key(const dag_hash_t privkey, dag_hash_t pubkey, uint8_t *pubkey_bit)
{
	uint8_t buf[sizeof(dag_hash_t) + 1];
	EC_KEY *eckey = 0;
	BIGNUM *priv = 0;
	EC_POINT *pub = 0;
	BN_CTX *ctx = 0;
	int res = -1;

	if(!group) {
		goto fail;
	}

	eckey = EC_KEY_new();
	if(!eckey) {
		goto fail;
	}

	if(!EC_KEY_set_group(eckey, group)) {
		goto fail;
	}

	priv = BN_new();
	if(!priv) {
		goto fail;
	}

	//	BN_init(priv);
	BN_bin2bn((uint8_t*)privkey, sizeof(dag_hash_t), priv);
	EC_KEY_set_private_key(eckey, priv);

	ctx = BN_CTX_new();
	if(!ctx) {
		goto fail;
	}

	BN_CTX_start(ctx);

	pub = EC_POINT_new(group);
	if(!pub) {
		goto fail;
	}

	EC_POINT_mul(group, pub, priv, NULL, NULL, ctx);
	EC_KEY_set_public_key(eckey, pub);
	if(EC_POINT_point2oct(group, pub, POINT_CONVERSION_COMPRESSED, buf, sizeof(dag_hash_t) + 1, ctx) != sizeof(dag_hash_t) + 1) {
		goto fail;
	}

	memcpy(pubkey, buf + 1, sizeof(dag_hash_t));
	*pubkey_bit = *buf & 1;
	res = 0;

fail:
	if(ctx) {
		BN_CTX_free(ctx);
	}

	if(priv) {
		BN_free(priv);
	}

	if(pub) {
		EC_POINT_free(pub);
	}

	if(res && eckey) {
		EC_KEY_free(eckey);
	}

	return res ? 0 : eckey;
}

// 通过已知的公钥返回密钥的内部表示
void *dag_public_to_key(const dag_hash_t pubkey, uint8_t pubkey_bit)
{
	EC_KEY *eckey = 0;
	BIGNUM *pub = 0;
	EC_POINT *p = 0;
	BN_CTX *ctx = 0;
	int res = -1;

	if(!group) {
		goto fail;
	}

	eckey = EC_KEY_new();
	if(!eckey) {
		goto fail;
	}

	if(!EC_KEY_set_group(eckey, group)) {
		goto fail;
	}

	pub = BN_new();
	if(!pub) {
		goto fail;
	}

	//	BN_init(pub);
	BN_bin2bn((uint8_t*)pubkey, sizeof(dag_hash_t), pub);
	p = EC_POINT_new(group);
	if(!p) {
		goto fail;
	}

	ctx = BN_CTX_new();
	if(!ctx) {
		goto fail;
	}

	BN_CTX_start(ctx);
	if(!EC_POINT_set_compressed_coordinates_GFp(group, p, pub, pubkey_bit, ctx)) {
		goto fail;
	}

	EC_KEY_set_public_key(eckey, p);
	res = 0;

fail:
	if(ctx) {
		BN_CTX_free(ctx);
	}

	if(pub) {
		BN_free(pub);
	}

	if(p) {
		EC_POINT_free(p);
	}

	if(res && eckey) {
		EC_KEY_free(eckey);
	}

	return res ? 0 : eckey;
}

// removes the internal key representation
void dag_free_key(void *key)
{
	EC_KEY_free((EC_KEY*)key);
}

// sign the hash and put the result in sign_r and sign_s
int dag_sign(const void *key, const dag_hash_t hash, dag_hash_t sign_r, dag_hash_t sign_s)
{
	uint8_t buf[72], *p;
	unsigned sig_len, s;

	if(!ECDSA_sign(0, (const uint8_t*)hash, sizeof(dag_hash_t), buf, &sig_len, (EC_KEY*)key)) {
		return -1;
	}

	p = buf + 3, s = *p++;

	if(s >= sizeof(dag_hash_t)) {
		memcpy(sign_r, p + s - sizeof(dag_hash_t), sizeof(dag_hash_t));
	} else {
		memset(sign_r, 0, sizeof(dag_hash_t));
		memcpy((uint8_t*)sign_r + sizeof(dag_hash_t) - s, p, s);
	}

	p += s + 1, s = *p++;

	if(s >= sizeof(dag_hash_t)) {
		memcpy(sign_s, p + s - sizeof(dag_hash_t), sizeof(dag_hash_t));
	} else {
		memset(sign_s, 0, sizeof(dag_hash_t));
		memcpy((uint8_t*)sign_s + sizeof(dag_hash_t) - s, p, s);
	}

	dag_debug("Sign  : hash=[%s] sign=[%s] r=[%s], s=[%s]", dag_log_hash(hash),
		xdag_log_array(buf, sig_len), dag_log_hash(sign_r), dag_log_hash(sign_s));

	return 0;
}

static uint8_t *add_number_to_sign(uint8_t *sign, const dag_hash_t num)
{
	uint8_t *n = (uint8_t*)num;
	int i, len, leadzero;

	for(i = 0; i < sizeof(dag_hash_t) && !n[i]; ++i);

	leadzero = (i < sizeof(dag_hash_t) && n[i] & 0x80);
	len = (sizeof(dag_hash_t) - i) + leadzero;
	*sign++ = 0x02;
	*sign++ = len;

	if(leadzero) {
		*sign++ = 0;
	}

	while(i < sizeof(dag_hash_t)) {
		*sign++ = n[i++];
	}

	return sign;
}

// 验证签名（sign_r，sign_s）是否对应于散列'hash'，即自己密钥的版本
// 成功时返回0
int dag_verify_signature(const void *key, const dag_hash_t hash, const dag_hash_t sign_r, const dag_hash_t sign_s)
{
	uint8_t buf[72], *ptr;
	int res;

	ptr = add_number_to_sign(buf + 2, sign_r);
	ptr = add_number_to_sign(ptr, sign_s);
	buf[0] = 0x30;
	buf[1] = ptr - buf - 2;
	res = ECDSA_verify(0, (const uint8_t*)hash, sizeof(dag_hash_t), buf, ptr - buf, (EC_KEY*)key);

	dag_debug("Verify: res=%2d key=%lx hash=[%s] sign=[%s] r=[%s], s=[%s]", res, (long)key, dag_log_hash(hash),
		dag_log_array(buf, ptr - buf), dag_log_hash(sign_r), dag_log_hash(sign_s));

	return res != 1;
}

#if USE_OPTIMIZED_EC == 1 || USE_OPTIMIZED_EC == 2

static uint8_t *add_number_to_sign_optimized_ec(uint8_t *sign, const dag_hash_t num)
{
	uint8_t *n = (uint8_t*)num;
	int i, len, leadzero;

	for(i = 0; i < sizeof(dag_hash_t) && !n[i]; ++i);

	leadzero = (i < sizeof(dag_hash_t) && n[i] & 0x80);
	len = (sizeof(dag_hash_t) - i) + leadzero;
	*sign++ = 0x02;
	if(len)
		*sign++ = len;
	else {
		*sign++ = 1;
		*sign++ = 0;
		return sign;
	}

	if(leadzero) {
		*sign++ = 0;
	}

	while(i < sizeof(dag_hash_t)) {
		*sign++ = n[i++];
	}

	return sign;
}

// 成功时返回0
int dag_verify_signature_optimized_ec(const void *key, const dag_hash_t hash, const dag_hash_t sign_r, const dag_hash_t sign_s)
{
	uint8_t buf_pubkey[sizeof(dag_hash_t) + 1];
	secp256k1_pubkey pubkey_noopenssl;
	size_t pubkeylen = sizeof(dag_hash_t) + 1;
	secp256k1_ecdsa_signature sig_noopenssl;
	secp256k1_ecdsa_signature sig_noopenssl_normalized;
	int res = 0;

	buf_pubkey[0] = 2 + ((uintptr_t)key & 1);
	memcpy(&(buf_pubkey[1]), (dag_hash_t*)((uintptr_t)key & ~1l), sizeof(dag_hash_t));

	if((res = secp256k1_ec_pubkey_parse(ctx_noopenssl, &pubkey_noopenssl, buf_pubkey, pubkeylen)) != 1) {
		dag_debug("Public key parsing failed: res=%2d key parity bit = %ld key=[%s] hash=[%s] r=[%s], s=[%s]", res, ((uintptr_t)key & 1),
			dag_log_hash((uint64_t*)((uintptr_t)key & ~1l)), dag_log_hash(hash), dag_log_hash(sign_r), dag_log_hash(sign_s));

	}

	uint8_t sign_buf[72], *ptr;

	ptr = add_number_to_sign_optimized_ec(sign_buf + 2, sign_r);
	ptr = add_number_to_sign_optimized_ec(ptr, sign_s);
	sign_buf[0] = 0x30;
	sign_buf[1] = ptr - sign_buf - 2;


	if((res = secp256k1_ecdsa_signature_parse_der(ctx_noopenssl, &sig_noopenssl, sign_buf, ptr - sign_buf)) != 1) {
		dag_debug("Signature parsing failed: res=%2d key parity bit = %ld key=[%s] hash=[%s] sign=[%s] r=[%s], s=[%s]", res, ((uintptr_t)key & 1),
			dag_log_hash((uint64_t*)((uintptr_t)key & ~1l)), dag_log_hash(hash),
			dag_log_array(sign_buf, ptr - sign_buf), dag_log_hash(sign_r), dag_log_hash(sign_s));
		return 1;
	}

	// never fail
	secp256k1_ecdsa_signature_normalize(ctx_noopenssl, &sig_noopenssl_normalized, &sig_noopenssl);

	if((res = secp256k1_ecdsa_verify(ctx_noopenssl, &sig_noopenssl_normalized, (unsigned char*)hash, &pubkey_noopenssl)) != 1) {
		dag_debug("Verify failed: res =%2d key parity bit = %ld key=[%s] hash=[%s] sign=[%s] r=[%s], s=[%s]", res, ((uintptr_t)key & 1),
			dag_log_hash((uint64_t*)((uintptr_t)key & ~1l)), dag_log_hash(hash),
			dag_log_array(sign_buf, ptr - sign_buf), dag_log_hash(sign_r), dag_log_hash(sign_s));
		return 1;
	}

	dag_debug("Verify completed: parity bit = %ld key=[%s] hash=[%s] sign=[%s] r=[%s], s=[%s]", ((uintptr_t)key & 1),
		dag_log_hash((uint64_t*)((uintptr_t)key & ~1l)), dag_log_hash(hash),
		dag_log_array(sign_buf, ptr - sign_buf), dag_log_hash(sign_r), dag_log_hash(sign_s));
	return 0;
}

#endif
