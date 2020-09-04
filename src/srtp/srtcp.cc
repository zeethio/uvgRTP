#include <cstring>
#include <iostream>

#include "srtp/srtcp.hh"

#ifdef __RTP_CRYPTO__
#include "crypto.hh"
#include <cryptopp/hex.h>
#endif

uvg_rtp::srtcp::srtcp()
{
}

uvg_rtp::srtcp::~srtcp()
{
}

#ifdef __RTP_CRYPTO__
rtp_error_t uvg_rtp::srtcp::encrypt(uint32_t ssrc, uint16_t seq, uint8_t *buffer, size_t len)
{
    if (use_null_cipher_)
        return RTP_OK;

    uint8_t iv[16] = { 0 };

    if (create_iv(iv, ssrc, seq, srtp_ctx_->key_ctx.local.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvg_rtp::crypto::aes::ctr ctr(srtp_ctx_->key_ctx.local.enc_key, sizeof(srtp_ctx_->key_ctx.local.enc_key), iv);
    ctr.encrypt(buffer, buffer, len);

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtcp::add_auth_tag(uint8_t *buffer, size_t len)
{
    auto hmac_sha1 = uvg_rtp::crypto::hmac::sha1(srtp_ctx_->key_ctx.local.auth_key, AES_KEY_LENGTH);

    hmac_sha1.update(buffer, len - AUTH_TAG_LENGTH);
    hmac_sha1.update((uint8_t *)&srtp_ctx_->roc, sizeof(srtp_ctx_->roc));
    hmac_sha1.final((uint8_t *)&buffer[len - AUTH_TAG_LENGTH], AUTH_TAG_LENGTH);

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtcp::verify_auth_tag(uint8_t *buffer, size_t len)
{
    uint8_t digest[10] = { 0 };
    auto hmac_sha1     = uvg_rtp::crypto::hmac::sha1(srtp_ctx_->key_ctx.remote.auth_key, AES_KEY_LENGTH);

    hmac_sha1.update(buffer, len - AUTH_TAG_LENGTH);
    hmac_sha1.update((uint8_t *)&srtp_ctx_->roc, sizeof(srtp_ctx_->roc));
    hmac_sha1.final(digest, AUTH_TAG_LENGTH);

    if (memcmp(digest, &buffer[len - AUTH_TAG_LENGTH], AUTH_TAG_LENGTH)) {
        LOG_ERROR("STCP authentication tag mismatch!");
        return RTP_AUTH_TAG_MISMATCH;
    }

    if (is_replayed_packet(digest)) {
        LOG_ERROR("Replayed packet received, discarding!");
        return RTP_INVALID_VALUE;
    }

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtcp::decrypt(uint32_t ssrc, uint32_t seq, uint8_t *buffer, size_t size)
{
    uint8_t iv[16]  = { 0 };

    if (create_iv(iv, ssrc, seq, srtp_ctx_->key_ctx.remote.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvg_rtp::crypto::aes::ctr ctr(srtp_ctx_->key_ctx.remote.enc_key, sizeof(srtp_ctx_->key_ctx.remote.enc_key), iv);

    /* skip header and sender ssrc */
    ctr.decrypt(&buffer[8], &buffer[8], size - 8 - AUTH_TAG_LENGTH - SRTCP_INDEX_LENGTH);
    return RTP_OK;
}
#endif
