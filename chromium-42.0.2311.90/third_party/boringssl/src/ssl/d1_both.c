/*
 * DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005. 
 */
/* ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <openssl/buf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include "ssl_locl.h"

#define RSMBLY_BITMASK_SIZE(msg_len) (((msg_len) + 7) / 8)

#define RSMBLY_BITMASK_MARK(bitmask, start, end)                     \
  {                                                                  \
    if ((end) - (start) <= 8) {                                      \
      long ii;                                                       \
      for (ii = (start); ii < (end); ii++)                           \
        bitmask[((ii) >> 3)] |= (1 << ((ii)&7));                     \
    } else {                                                         \
      long ii;                                                       \
      bitmask[((start) >> 3)] |= bitmask_start_values[((start)&7)];  \
      for (ii = (((start) >> 3) + 1); ii < ((((end)-1)) >> 3); ii++) \
        bitmask[ii] = 0xff;                                          \
      bitmask[(((end)-1) >> 3)] |= bitmask_end_values[((end)&7)];    \
    }                                                                \
  }

#define RSMBLY_BITMASK_IS_COMPLETE(bitmask, msg_len, is_complete)           \
  {                                                                         \
    long ii;                                                                \
    assert((msg_len) > 0);                                                  \
    is_complete = 1;                                                        \
    if (bitmask[(((msg_len)-1) >> 3)] != bitmask_end_values[((msg_len)&7)]) \
      is_complete = 0;                                                      \
    if (is_complete)                                                        \
      for (ii = (((msg_len)-1) >> 3) - 1; ii >= 0; ii--)                    \
        if (bitmask[ii] != 0xff) {                                          \
          is_complete = 0;                                                  \
          break;                                                            \
        }                                                                   \
  }

static const uint8_t bitmask_start_values[] = {0xff, 0xfe, 0xfc, 0xf8,
                                               0xf0, 0xe0, 0xc0, 0x80};
static const uint8_t bitmask_end_values[] = {0xff, 0x01, 0x03, 0x07,
                                             0x0f, 0x1f, 0x3f, 0x7f};

/* TODO(davidben): 28 comes from the size of IP + UDP header. Is this reasonable
 * for these values? Notably, why is kMinMTU a function of the transport
 * protocol's overhead rather than, say, what's needed to hold a minimally-sized
 * handshake fragment plus protocol overhead. */

/* kMinMTU is the minimum acceptable MTU value. */
static const unsigned int kMinMTU = 256 - 28;

/* kDefaultMTU is the default MTU value to use if neither the user nor
 * the underlying BIO supplies one. */
static const unsigned int kDefaultMTU = 1500 - 28;

static void dtls1_fix_message_header(SSL *s, unsigned long frag_off,
                                     unsigned long frag_len);
static unsigned char *dtls1_write_message_header(SSL *s, unsigned char *p);
static long dtls1_get_message_fragment(SSL *s, int stn, long max, int *ok);

static hm_fragment *dtls1_hm_fragment_new(unsigned long frag_len,
                                          int reassembly) {
  hm_fragment *frag = NULL;
  unsigned char *buf = NULL;
  unsigned char *bitmask = NULL;

  frag = (hm_fragment *)OPENSSL_malloc(sizeof(hm_fragment));
  if (frag == NULL) {
    return NULL;
  }

  if (frag_len) {
    buf = (unsigned char *)OPENSSL_malloc(frag_len);
    if (buf == NULL) {
      OPENSSL_free(frag);
      return NULL;
    }
  }

  /* zero length fragment gets zero frag->fragment */
  frag->fragment = buf;

  /* Initialize reassembly bitmask if necessary */
  if (reassembly) {
    bitmask = (unsigned char *)OPENSSL_malloc(RSMBLY_BITMASK_SIZE(frag_len));
    if (bitmask == NULL) {
      if (buf != NULL) {
        OPENSSL_free(buf);
      }
      OPENSSL_free(frag);
      return NULL;
    }
    memset(bitmask, 0, RSMBLY_BITMASK_SIZE(frag_len));
  }

  frag->reassembly = bitmask;

  return frag;
}

void dtls1_hm_fragment_free(hm_fragment *frag) {
  if (frag->fragment) {
    OPENSSL_free(frag->fragment);
  }
  if (frag->reassembly) {
    OPENSSL_free(frag->reassembly);
  }
  OPENSSL_free(frag);
}

/* send s->init_buf in records of type 'type' (SSL3_RT_HANDSHAKE or
 * SSL3_RT_CHANGE_CIPHER_SPEC) */
int dtls1_do_write(SSL *s, int type) {
  int ret;
  int curr_mtu;
  unsigned int len, frag_off;
  size_t max_overhead = 0;

  /* AHA!  Figure out the MTU, and stick to the right size */
  if (s->d1->mtu < dtls1_min_mtu() &&
      !(SSL_get_options(s) & SSL_OP_NO_QUERY_MTU)) {
    long mtu = BIO_ctrl(SSL_get_wbio(s), BIO_CTRL_DGRAM_QUERY_MTU, 0, NULL);
    if (mtu >= 0 && mtu <= (1 << 30) && (unsigned)mtu >= dtls1_min_mtu()) {
      s->d1->mtu = (unsigned)mtu;
    } else {
      s->d1->mtu = kDefaultMTU;
      BIO_ctrl(SSL_get_wbio(s), BIO_CTRL_DGRAM_SET_MTU, s->d1->mtu, NULL);
    }
  }

  /* should have something reasonable now */
  assert(s->d1->mtu >= dtls1_min_mtu());

  if (s->init_off == 0 && type == SSL3_RT_HANDSHAKE) {
    assert(s->init_num ==
           (int)s->d1->w_msg_hdr.msg_len + DTLS1_HM_HEADER_LENGTH);
  }

  /* Determine the maximum overhead of the current cipher. */
  if (s->aead_write_ctx != NULL) {
    max_overhead = EVP_AEAD_max_overhead(s->aead_write_ctx->ctx.aead);
    if (s->aead_write_ctx->variable_nonce_included_in_record) {
      max_overhead += s->aead_write_ctx->variable_nonce_len;
    }
  }

  frag_off = 0;
  while (s->init_num) {
    /* Account for data in the buffering BIO; multiple records may be packed
     * into a single packet during the handshake.
     *
     * TODO(davidben): This is buggy; if the MTU is larger than the buffer size,
     * the large record will be split across two packets. Moreover, in that
     * case, the |dtls1_write_bytes| call may not return synchronously. This
     * will break on retry as the |s->init_off| and |s->init_num| adjustment
     * will run a second time. */
    curr_mtu = s->d1->mtu - BIO_wpending(SSL_get_wbio(s)) -
        DTLS1_RT_HEADER_LENGTH - max_overhead;

    if (curr_mtu <= DTLS1_HM_HEADER_LENGTH) {
      /* Flush the buffer and continue with a fresh packet.
       *
       * TODO(davidben): If |BIO_flush| is not synchronous and requires multiple
       * calls to |dtls1_do_write|, |frag_off| will be wrong. */
      ret = BIO_flush(SSL_get_wbio(s));
      if (ret <= 0) {
        return ret;
      }
      assert(BIO_wpending(SSL_get_wbio(s)) == 0);
      curr_mtu = s->d1->mtu - DTLS1_RT_HEADER_LENGTH - max_overhead;
    }

    /* XDTLS: this function is too long.  split out the CCS part */
    if (type == SSL3_RT_HANDSHAKE) {
      /* If this isn't the first fragment, reserve space to prepend a new
       * fragment header. This will override the body of a previous fragment. */
      if (s->init_off != 0) {
        assert(s->init_off > DTLS1_HM_HEADER_LENGTH);
        s->init_off -= DTLS1_HM_HEADER_LENGTH;
        s->init_num += DTLS1_HM_HEADER_LENGTH;
      }

      if (curr_mtu <= DTLS1_HM_HEADER_LENGTH) {
        /* To make forward progress, the MTU must, at minimum, fit the handshake
         * header and one byte of handshake body. */
        OPENSSL_PUT_ERROR(SSL, dtls1_do_write, SSL_R_MTU_TOO_SMALL);
        return -1;
      }

      if (s->init_num > curr_mtu) {
        len = curr_mtu;
      } else {
        len = s->init_num;
      }
      assert(len >= DTLS1_HM_HEADER_LENGTH);

      dtls1_fix_message_header(s, frag_off, len - DTLS1_HM_HEADER_LENGTH);
      dtls1_write_message_header(
          s, (uint8_t *)&s->init_buf->data[s->init_off]);
    } else {
      assert(type == SSL3_RT_CHANGE_CIPHER_SPEC);
      /* ChangeCipherSpec cannot be fragmented. */
      if (s->init_num > curr_mtu) {
        OPENSSL_PUT_ERROR(SSL, dtls1_do_write, SSL_R_MTU_TOO_SMALL);
        return -1;
      }
      len = s->init_num;
    }

    ret = dtls1_write_bytes(s, type, &s->init_buf->data[s->init_off], len);
    if (ret < 0) {
      return -1;
    }

    /* bad if this assert fails, only part of the handshake message got sent.
     * But why would this happen? */
    assert(len == (unsigned int)ret);

    if (ret == s->init_num) {
      if (s->msg_callback) {
        s->msg_callback(1, s->version, type, s->init_buf->data,
                        (size_t)(s->init_off + s->init_num), s,
                        s->msg_callback_arg);
      }

      s->init_off = 0; /* done writing this message */
      s->init_num = 0;

      return 1;
    }
    s->init_off += ret;
    s->init_num -= ret;
    frag_off += (ret -= DTLS1_HM_HEADER_LENGTH);
  }

  return 0;
}


/* Obtain handshake message of message type 'mt' (any if mt == -1), maximum
 * acceptable body length 'max'. Read an entire handshake message. Handshake
 * messages arrive in fragments. */
long dtls1_get_message(SSL *s, int st1, int stn, int mt, long max,
                       int hash_message, int *ok) {
  int i, al;
  struct hm_header_st *msg_hdr;
  uint8_t *p;
  unsigned long msg_len;

  /* s3->tmp is used to store messages that are unexpected, caused
   * by the absence of an optional handshake message */
  if (s->s3->tmp.reuse_message) {
    /* A SSL_GET_MESSAGE_DONT_HASH_MESSAGE call cannot be combined
     * with reuse_message; the SSL_GET_MESSAGE_DONT_HASH_MESSAGE
     * would have to have been applied to the previous call. */
    assert(hash_message != SSL_GET_MESSAGE_DONT_HASH_MESSAGE);
    s->s3->tmp.reuse_message = 0;
    if (mt >= 0 && s->s3->tmp.message_type != mt) {
      al = SSL_AD_UNEXPECTED_MESSAGE;
      OPENSSL_PUT_ERROR(SSL, dtls1_get_message, SSL_R_UNEXPECTED_MESSAGE);
      goto f_err;
    }
    *ok = 1;
    s->init_msg = (uint8_t *)s->init_buf->data + DTLS1_HM_HEADER_LENGTH;
    s->init_num = (int)s->s3->tmp.message_size;
    return s->init_num;
  }

  msg_hdr = &s->d1->r_msg_hdr;
  memset(msg_hdr, 0x00, sizeof(struct hm_header_st));

again:
  i = dtls1_get_message_fragment(s, stn, max, ok);
  if (i == DTLS1_HM_BAD_FRAGMENT ||
      i == DTLS1_HM_FRAGMENT_RETRY) {
    /* bad fragment received */
    goto again;
  } else if (i <= 0 && !*ok) {
    return i;
  }

  p = (uint8_t *)s->init_buf->data;
  msg_len = msg_hdr->msg_len;

  /* reconstruct message header */
  *(p++) = msg_hdr->type;
  l2n3(msg_len, p);
  s2n(msg_hdr->seq, p);
  l2n3(0, p);
  l2n3(msg_len, p);
  p -= DTLS1_HM_HEADER_LENGTH;
  msg_len += DTLS1_HM_HEADER_LENGTH;

  s->init_msg = (uint8_t *)s->init_buf->data + DTLS1_HM_HEADER_LENGTH;

  if (hash_message != SSL_GET_MESSAGE_DONT_HASH_MESSAGE &&
      !ssl3_hash_current_message(s)) {
    goto err;
  }
  if (s->msg_callback) {
    s->msg_callback(0, s->version, SSL3_RT_HANDSHAKE, p, msg_len, s,
                    s->msg_callback_arg);
  }

  memset(msg_hdr, 0x00, sizeof(struct hm_header_st));

  s->d1->handshake_read_seq++;

  return s->init_num;

f_err:
  ssl3_send_alert(s, SSL3_AL_FATAL, al);
err:
  *ok = 0;
  return -1;
}

static int dtls1_preprocess_fragment(SSL *s, struct hm_header_st *msg_hdr,
                                     int max) {
  size_t frag_off, frag_len, msg_len;

  msg_len = msg_hdr->msg_len;
  frag_off = msg_hdr->frag_off;
  frag_len = msg_hdr->frag_len;

  /* sanity checking */
  if ((frag_off + frag_len) > msg_len) {
    OPENSSL_PUT_ERROR(SSL, dtls1_preprocess_fragment,
                      SSL_R_EXCESSIVE_MESSAGE_SIZE);
    return SSL_AD_ILLEGAL_PARAMETER;
  }

  if ((frag_off + frag_len) > (unsigned long)max) {
    OPENSSL_PUT_ERROR(SSL, dtls1_preprocess_fragment,
                      SSL_R_EXCESSIVE_MESSAGE_SIZE);
    return SSL_AD_ILLEGAL_PARAMETER;
  }

  if (s->d1->r_msg_hdr.frag_off == 0) {
    /* first fragment */
    /* msg_len is limited to 2^24, but is effectively checked
     * against max above */
    if (!BUF_MEM_grow_clean(s->init_buf, msg_len + DTLS1_HM_HEADER_LENGTH)) {
      OPENSSL_PUT_ERROR(SSL, dtls1_preprocess_fragment, ERR_R_BUF_LIB);
      return SSL_AD_INTERNAL_ERROR;
    }

    s->s3->tmp.message_size = msg_len;
    s->d1->r_msg_hdr.msg_len = msg_len;
    s->s3->tmp.message_type = msg_hdr->type;
    s->d1->r_msg_hdr.type = msg_hdr->type;
    s->d1->r_msg_hdr.seq = msg_hdr->seq;
  } else if (msg_len != s->d1->r_msg_hdr.msg_len) {
    /* They must be playing with us! BTW, failure to enforce
     * upper limit would open possibility for buffer overrun. */
    OPENSSL_PUT_ERROR(SSL, dtls1_preprocess_fragment,
                      SSL_R_EXCESSIVE_MESSAGE_SIZE);
    return SSL_AD_ILLEGAL_PARAMETER;
  }

  return 0; /* no error */
}


static int dtls1_retrieve_buffered_fragment(SSL *s, long max, int *ok) {
  /* (0) check whether the desired fragment is available
   * if so:
   * (1) copy over the fragment to s->init_buf->data[]
   * (2) update s->init_num */
  pitem *item;
  hm_fragment *frag;
  int al;
  unsigned long frag_len;

  *ok = 0;
  item = pqueue_peek(s->d1->buffered_messages);
  if (item == NULL) {
    return 0;
  }

  frag = (hm_fragment *)item->data;

  /* Don't return if reassembly still in progress */
  if (frag->reassembly != NULL) {
    return 0;
  }

  if (s->d1->handshake_read_seq != frag->msg_header.seq) {
    return 0;
  }

  frag_len = frag->msg_header.frag_len;
  pqueue_pop(s->d1->buffered_messages);

  al = dtls1_preprocess_fragment(s, &frag->msg_header, max);

  if (al == 0) {
    /* no alert */
    uint8_t *p = (uint8_t *)s->init_buf->data + DTLS1_HM_HEADER_LENGTH;
    memcpy(&p[frag->msg_header.frag_off], frag->fragment,
           frag->msg_header.frag_len);
  }

  dtls1_hm_fragment_free(frag);
  pitem_free(item);

  if (al == 0) {
    *ok = 1;
    return frag_len;
  }

  ssl3_send_alert(s, SSL3_AL_FATAL, al);
  s->init_num = 0;
  *ok = 0;
  return -1;
}

/* dtls1_max_handshake_message_len returns the maximum number of bytes
 * permitted in a DTLS handshake message for |s|. The minimum is 16KB, but may
 * be greater if the maximum certificate list size requires it. */
static unsigned long dtls1_max_handshake_message_len(const SSL *s) {
  unsigned long max_len = DTLS1_HM_HEADER_LENGTH + SSL3_RT_MAX_ENCRYPTED_LENGTH;
  if (max_len < (unsigned long)s->max_cert_list) {
    return s->max_cert_list;
  }
  return max_len;
}

static int dtls1_reassemble_fragment(SSL *s, const struct hm_header_st *msg_hdr,
                                     int *ok) {
  hm_fragment *frag = NULL;
  pitem *item = NULL;
  int i = -1, is_complete;
  uint8_t seq64be[8];
  unsigned long frag_len = msg_hdr->frag_len;

  if ((msg_hdr->frag_off + frag_len) > msg_hdr->msg_len ||
      msg_hdr->msg_len > dtls1_max_handshake_message_len(s)) {
    goto err;
  }

  if (frag_len == 0) {
    return DTLS1_HM_FRAGMENT_RETRY;
  }

  /* Try to find item in queue */
  memset(seq64be, 0, sizeof(seq64be));
  seq64be[6] = (uint8_t)(msg_hdr->seq >> 8);
  seq64be[7] = (uint8_t)msg_hdr->seq;
  item = pqueue_find(s->d1->buffered_messages, seq64be);

  if (item == NULL) {
    frag = dtls1_hm_fragment_new(msg_hdr->msg_len, 1);
    if (frag == NULL) {
      goto err;
    }
    memcpy(&(frag->msg_header), msg_hdr, sizeof(*msg_hdr));
    frag->msg_header.frag_len = frag->msg_header.msg_len;
    frag->msg_header.frag_off = 0;
  } else {
    frag = (hm_fragment *)item->data;
    if (frag->msg_header.msg_len != msg_hdr->msg_len) {
      item = NULL;
      frag = NULL;
      goto err;
    }
  }

  /* If message is already reassembled, this must be a
   * retransmit and can be dropped. In this case item != NULL and so frag
   * does not need to be freed. */
  if (frag->reassembly == NULL) {
    uint8_t devnull[256];

    assert(item != NULL);
    while (frag_len) {
      i = s->method->ssl_read_bytes(
          s, SSL3_RT_HANDSHAKE, devnull,
          frag_len > sizeof(devnull) ? sizeof(devnull) : frag_len, 0);
      if (i <= 0) {
        goto err;
      }
      frag_len -= i;
    }
    return DTLS1_HM_FRAGMENT_RETRY;
  }

  /* read the body of the fragment (header has already been read */
  i = s->method->ssl_read_bytes(
      s, SSL3_RT_HANDSHAKE, frag->fragment + msg_hdr->frag_off, frag_len, 0);
  if ((unsigned long)i != frag_len) {
    i = -1;
  }
  if (i <= 0) {
    goto err;
  }

  RSMBLY_BITMASK_MARK(frag->reassembly, (long)msg_hdr->frag_off,
                      (long)(msg_hdr->frag_off + frag_len));

  RSMBLY_BITMASK_IS_COMPLETE(frag->reassembly, (long)msg_hdr->msg_len,
                             is_complete);

  if (is_complete) {
    OPENSSL_free(frag->reassembly);
    frag->reassembly = NULL;
  }

  if (item == NULL) {
    item = pitem_new(seq64be, frag);
    if (item == NULL) {
      i = -1;
      goto err;
    }

    item = pqueue_insert(s->d1->buffered_messages, item);
    /* pqueue_insert fails iff a duplicate item is inserted.
     * However, |item| cannot be a duplicate. If it were,
     * |pqueue_find|, above, would have returned it and control
     * would never have reached this branch. */
    assert(item != NULL);
  }

  return DTLS1_HM_FRAGMENT_RETRY;

err:
  if (frag != NULL && item == NULL) {
    dtls1_hm_fragment_free(frag);
  }
  *ok = 0;
  return i;
}

static int dtls1_process_out_of_seq_message(SSL *s,
                                            const struct hm_header_st *msg_hdr,
                                            int *ok) {
  int i = -1;
  hm_fragment *frag = NULL;
  pitem *item = NULL;
  uint8_t seq64be[8];
  unsigned long frag_len = msg_hdr->frag_len;

  if ((msg_hdr->frag_off + frag_len) > msg_hdr->msg_len) {
    goto err;
  }

  /* Try to find item in queue, to prevent duplicate entries */
  memset(seq64be, 0, sizeof(seq64be));
  seq64be[6] = (uint8_t)(msg_hdr->seq >> 8);
  seq64be[7] = (uint8_t)msg_hdr->seq;
  item = pqueue_find(s->d1->buffered_messages, seq64be);

  /* If we already have an entry and this one is a fragment,
   * don't discard it and rather try to reassemble it. */
  if (item != NULL && frag_len != msg_hdr->msg_len) {
    item = NULL;
  }

  /* Discard the message if sequence number was already there, is
   * too far in the future, or already in the queue. */
  if (msg_hdr->seq <= s->d1->handshake_read_seq ||
      msg_hdr->seq > s->d1->handshake_read_seq + 10 || item != NULL) {
    uint8_t devnull[256];

    while (frag_len) {
      i = s->method->ssl_read_bytes(
          s, SSL3_RT_HANDSHAKE, devnull,
          frag_len > sizeof(devnull) ? sizeof(devnull) : frag_len, 0);
      if (i <= 0) {
        goto err;
      }
      frag_len -= i;
    }
  } else {
    if (frag_len != msg_hdr->msg_len) {
      return dtls1_reassemble_fragment(s, msg_hdr, ok);
    }

    if (frag_len > dtls1_max_handshake_message_len(s)) {
      goto err;
    }

    frag = dtls1_hm_fragment_new(frag_len, 0);
    if (frag == NULL) {
      goto err;
    }

    memcpy(&(frag->msg_header), msg_hdr, sizeof(*msg_hdr));

    if (frag_len) {
      /* read the body of the fragment (header has already been read */
      i = s->method->ssl_read_bytes(s, SSL3_RT_HANDSHAKE, frag->fragment,
                                    frag_len, 0);
      if ((unsigned long)i != frag_len) {
        i = -1;
      }
      if (i <= 0) {
        goto err;
      }
    }

    item = pitem_new(seq64be, frag);
    if (item == NULL) {
      goto err;
    }

    item = pqueue_insert(s->d1->buffered_messages, item);
    /* pqueue_insert fails iff a duplicate item is inserted.
     * However, |item| cannot be a duplicate. If it were,
     * |pqueue_find|, above, would have returned it. Then, either
     * |frag_len| != |msg_hdr->msg_len| in which case |item| is set
     * to NULL and it will have been processed with
     * |dtls1_reassemble_fragment|, above, or the record will have
     * been discarded. */
    assert(item != NULL);
  }

  return DTLS1_HM_FRAGMENT_RETRY;

err:
  if (frag != NULL && item == NULL) {
    dtls1_hm_fragment_free(frag);
  }
  *ok = 0;
  return i;
}


static long dtls1_get_message_fragment(SSL *s, int stn, long max, int *ok) {
  uint8_t wire[DTLS1_HM_HEADER_LENGTH];
  unsigned long len, frag_off, frag_len;
  int i, al;
  struct hm_header_st msg_hdr;

redo:
  /* see if we have the required fragment already */
  if ((frag_len = dtls1_retrieve_buffered_fragment(s, max, ok)) || *ok) {
    if (*ok) {
      s->init_num = frag_len;
    }
    return frag_len;
  }

  /* read handshake message header */
  i = s->method->ssl_read_bytes(s, SSL3_RT_HANDSHAKE, wire,
                                DTLS1_HM_HEADER_LENGTH, 0);
  if (i <= 0) {
    /* nbio, or an error */
    s->rwstate = SSL_READING;
    *ok = 0;
    return i;
  }

  /* Handshake fails if message header is incomplete */
  if (i != DTLS1_HM_HEADER_LENGTH) {
    al = SSL_AD_UNEXPECTED_MESSAGE;
    OPENSSL_PUT_ERROR(SSL, dtls1_get_message_fragment,
                      SSL_R_UNEXPECTED_MESSAGE);
    goto f_err;
  }

  /* parse the message fragment header */
  dtls1_get_message_header(wire, &msg_hdr);

  /* if this is a future (or stale) message it gets buffered
   * (or dropped)--no further processing at this time. */
  if (msg_hdr.seq != s->d1->handshake_read_seq) {
    return dtls1_process_out_of_seq_message(s, &msg_hdr, ok);
  }

  len = msg_hdr.msg_len;
  frag_off = msg_hdr.frag_off;
  frag_len = msg_hdr.frag_len;

  if (frag_len && frag_len < len) {
    return dtls1_reassemble_fragment(s, &msg_hdr, ok);
  }

  if (!s->server && s->d1->r_msg_hdr.frag_off == 0 &&
      wire[0] == SSL3_MT_HELLO_REQUEST) {
    /* The server may always send 'Hello Request' messages --
     * we are doing a handshake anyway now, so ignore them
     * if their format is correct. Does not count for
     * 'Finished' MAC. */
    if (wire[1] == 0 && wire[2] == 0 && wire[3] == 0) {
      if (s->msg_callback) {
        s->msg_callback(0, s->version, SSL3_RT_HANDSHAKE, wire,
                        DTLS1_HM_HEADER_LENGTH, s, s->msg_callback_arg);
      }

      s->init_num = 0;
      goto redo;
    } else {
      /* Incorrectly formated Hello request */
      al = SSL_AD_UNEXPECTED_MESSAGE;
      OPENSSL_PUT_ERROR(SSL, dtls1_get_message_fragment,
                        SSL_R_UNEXPECTED_MESSAGE);
      goto f_err;
    }
  }

  if ((al = dtls1_preprocess_fragment(s, &msg_hdr, max))) {
    goto f_err;
  }

  /* XDTLS:  ressurect this when restart is in place */
  s->state = stn;

  if (frag_len > 0) {
    uint8_t *p = (uint8_t *)s->init_buf->data + DTLS1_HM_HEADER_LENGTH;

    i = s->method->ssl_read_bytes(s, SSL3_RT_HANDSHAKE, &p[frag_off], frag_len,
                                  0);
    /* XDTLS:  fix this--message fragments cannot span multiple packets */
    if (i <= 0) {
      s->rwstate = SSL_READING;
      *ok = 0;
      return i;
    }
  } else {
    i = 0;
  }

  /* XDTLS:  an incorrectly formatted fragment should cause the
   * handshake to fail */
  if (i != (int)frag_len) {
    al = SSL3_AD_ILLEGAL_PARAMETER;
    OPENSSL_PUT_ERROR(SSL, dtls1_get_message_fragment,
                      SSL3_AD_ILLEGAL_PARAMETER);
    goto f_err;
  }

  *ok = 1;

  /* Note that s->init_num is *not* used as current offset in
   * s->init_buf->data, but as a counter summing up fragments'
   * lengths: as soon as they sum up to handshake packet
   * length, we assume we have got all the fragments. */
  s->init_num = frag_len;
  return frag_len;

f_err:
  ssl3_send_alert(s, SSL3_AL_FATAL, al);
  s->init_num = 0;

  *ok = 0;
  return -1;
}

/* for these 2 messages, we need to
 * ssl->enc_read_ctx			re-init
 * ssl->s3->read_sequence		zero
 * ssl->s3->read_mac_secret		re-init
 * ssl->session->read_sym_enc		assign
 * ssl->session->read_compression	assign
 * ssl->session->read_hash		assign */
int dtls1_send_change_cipher_spec(SSL *s, int a, int b) {
  uint8_t *p;

  if (s->state == a) {
    p = (uint8_t *)s->init_buf->data;
    *p++ = SSL3_MT_CCS;
    s->d1->handshake_write_seq = s->d1->next_handshake_write_seq;
    s->init_num = DTLS1_CCS_HEADER_LENGTH;

    s->init_off = 0;

    dtls1_set_message_header(s, SSL3_MT_CCS, 0, s->d1->handshake_write_seq, 0,
                             0);

    /* buffer the message to handle re-xmits */
    dtls1_buffer_message(s, 1);

    s->state = b;
  }

  /* SSL3_ST_CW_CHANGE_B */
  return dtls1_do_write(s, SSL3_RT_CHANGE_CIPHER_SPEC);
}

int dtls1_read_failed(SSL *s, int code) {
  if (code > 0) {
    fprintf(stderr, "invalid state reached %s:%d", __FILE__, __LINE__);
    return 1;
  }

  if (!dtls1_is_timer_expired(s)) {
    /* not a timeout, none of our business, let higher layers handle this. In
     * fact, it's probably an error */
    return code;
  }

  if (!SSL_in_init(s)) {
    /* done, no need to send a retransmit */
    BIO_set_flags(SSL_get_rbio(s), BIO_FLAGS_READ);
    return code;
  }

  return dtls1_handle_timeout(s);
}

int dtls1_get_queue_priority(unsigned short seq, int is_ccs) {
  /* The index of the retransmission queue actually is the message sequence
   * number, since the queue only contains messages of a single handshake.
   * However, the ChangeCipherSpec has no message sequence number and so using
   * only the sequence will result in the CCS and Finished having the same
   * index. To prevent this, the sequence number is multiplied by 2. In case of
   * a CCS 1 is subtracted. This does not only differ CSS and Finished, it also
   * maintains the order of the index (important for priority queues) and fits
   * in the unsigned short variable. */
  return seq * 2 - is_ccs;
}

int dtls1_retransmit_buffered_messages(SSL *s) {
  pqueue sent = s->d1->sent_messages;
  piterator iter;
  pitem *item;
  hm_fragment *frag;
  int found = 0;

  iter = pqueue_iterator(sent);

  for (item = pqueue_next(&iter); item != NULL; item = pqueue_next(&iter)) {
    frag = (hm_fragment *)item->data;
    if (dtls1_retransmit_message(
            s, (unsigned short)dtls1_get_queue_priority(
                   frag->msg_header.seq, frag->msg_header.is_ccs),
            0, &found) <= 0 &&
        found) {
      fprintf(stderr, "dtls1_retransmit_message() failed\n");
      return -1;
    }
  }

  return 1;
}

int dtls1_buffer_message(SSL *s, int is_ccs) {
  pitem *item;
  hm_fragment *frag;
  uint8_t seq64be[8];

  /* this function is called immediately after a message has
   * been serialized */
  assert(s->init_off == 0);

  frag = dtls1_hm_fragment_new(s->init_num, 0);
  if (!frag) {
    return 0;
  }

  memcpy(frag->fragment, s->init_buf->data, s->init_num);

  if (is_ccs) {
    assert(s->d1->w_msg_hdr.msg_len + DTLS1_CCS_HEADER_LENGTH ==
           (unsigned int)s->init_num);
  } else {
    assert(s->d1->w_msg_hdr.msg_len + DTLS1_HM_HEADER_LENGTH ==
           (unsigned int)s->init_num);
  }

  frag->msg_header.msg_len = s->d1->w_msg_hdr.msg_len;
  frag->msg_header.seq = s->d1->w_msg_hdr.seq;
  frag->msg_header.type = s->d1->w_msg_hdr.type;
  frag->msg_header.frag_off = 0;
  frag->msg_header.frag_len = s->d1->w_msg_hdr.msg_len;
  frag->msg_header.is_ccs = is_ccs;
  frag->msg_header.epoch = s->d1->w_epoch;

  memset(seq64be, 0, sizeof(seq64be));
  seq64be[6] = (uint8_t)(
      dtls1_get_queue_priority(frag->msg_header.seq, frag->msg_header.is_ccs) >>
      8);
  seq64be[7] = (uint8_t)(
      dtls1_get_queue_priority(frag->msg_header.seq, frag->msg_header.is_ccs));

  item = pitem_new(seq64be, frag);
  if (item == NULL) {
    dtls1_hm_fragment_free(frag);
    return 0;
  }

  pqueue_insert(s->d1->sent_messages, item);
  return 1;
}

int dtls1_retransmit_message(SSL *s, unsigned short seq, unsigned long frag_off,
                             int *found) {
  int ret;
  /* XDTLS: for now assuming that read/writes are blocking */
  pitem *item;
  hm_fragment *frag;
  unsigned long header_length;
  uint8_t seq64be[8];
  uint8_t save_write_sequence[8];

  /* assert(s->init_num == 0);
     assert(s->init_off == 0); */

  /* XDTLS:  the requested message ought to be found, otherwise error */
  memset(seq64be, 0, sizeof(seq64be));
  seq64be[6] = (uint8_t)(seq >> 8);
  seq64be[7] = (uint8_t)seq;

  item = pqueue_find(s->d1->sent_messages, seq64be);
  if (item == NULL) {
    fprintf(stderr, "retransmit:  message %d non-existant\n", seq);
    *found = 0;
    return 0;
  }

  *found = 1;
  frag = (hm_fragment *)item->data;

  if (frag->msg_header.is_ccs) {
    header_length = DTLS1_CCS_HEADER_LENGTH;
  } else {
    header_length = DTLS1_HM_HEADER_LENGTH;
  }

  memcpy(s->init_buf->data, frag->fragment,
         frag->msg_header.msg_len + header_length);
  s->init_num = frag->msg_header.msg_len + header_length;

  dtls1_set_message_header(s, frag->msg_header.type,
                           frag->msg_header.msg_len, frag->msg_header.seq,
                           0, frag->msg_header.frag_len);

  /* Save current state. */
  SSL_AEAD_CTX *aead_write_ctx = s->aead_write_ctx;
  uint16_t epoch = s->d1->w_epoch;

  /* DTLS renegotiation is unsupported, so only epochs 0 (NULL cipher) and 1
   * (negotiated cipher) exist. */
  assert(epoch == 0 || epoch == 1);
  assert(frag->msg_header.epoch <= epoch);
  const int fragment_from_previous_epoch = (epoch == 1 &&
                                            frag->msg_header.epoch == 0);
  if (fragment_from_previous_epoch) {
    /* Rewind to the previous epoch.
     *
     * TODO(davidben): Instead of swapping out connection-global state, this
     * logic should pass a "use previous epoch" parameter down to lower-level
     * functions. */
    s->d1->w_epoch = frag->msg_header.epoch;
    s->aead_write_ctx = NULL;
    memcpy(save_write_sequence, s->s3->write_sequence,
           sizeof(s->s3->write_sequence));
    memcpy(s->s3->write_sequence, s->d1->last_write_sequence,
           sizeof(s->s3->write_sequence));
  } else {
    /* Otherwise the messages must be from the same epoch. */
    assert(frag->msg_header.epoch == epoch);
  }

  ret = dtls1_do_write(s, frag->msg_header.is_ccs ? SSL3_RT_CHANGE_CIPHER_SPEC
                                                  : SSL3_RT_HANDSHAKE);

  if (fragment_from_previous_epoch) {
    /* Restore the current epoch. */
    s->aead_write_ctx = aead_write_ctx;
    s->d1->w_epoch = epoch;
    memcpy(s->d1->last_write_sequence, s->s3->write_sequence,
           sizeof(s->s3->write_sequence));
    memcpy(s->s3->write_sequence, save_write_sequence,
           sizeof(s->s3->write_sequence));
  }

  (void)BIO_flush(SSL_get_wbio(s));
  return ret;
}

/* call this function when the buffered messages are no longer needed */
void dtls1_clear_record_buffer(SSL *s) {
  pitem *item;

  for (item = pqueue_pop(s->d1->sent_messages); item != NULL;
       item = pqueue_pop(s->d1->sent_messages)) {
    dtls1_hm_fragment_free((hm_fragment *)item->data);
    pitem_free(item);
  }
}

/* don't actually do the writing, wait till the MTU has been retrieved */
void dtls1_set_message_header(SSL *s, uint8_t mt, unsigned long len,
                              unsigned short seq_num, unsigned long frag_off,
                              unsigned long frag_len) {
  struct hm_header_st *msg_hdr = &s->d1->w_msg_hdr;

  msg_hdr->type = mt;
  msg_hdr->msg_len = len;
  msg_hdr->seq = seq_num;
  msg_hdr->frag_off = frag_off;
  msg_hdr->frag_len = frag_len;
}

static void dtls1_fix_message_header(SSL *s, unsigned long frag_off,
                                     unsigned long frag_len) {
  struct hm_header_st *msg_hdr = &s->d1->w_msg_hdr;

  msg_hdr->frag_off = frag_off;
  msg_hdr->frag_len = frag_len;
}

static uint8_t *dtls1_write_message_header(SSL *s, uint8_t *p) {
  struct hm_header_st *msg_hdr = &s->d1->w_msg_hdr;

  *p++ = msg_hdr->type;
  l2n3(msg_hdr->msg_len, p);

  s2n(msg_hdr->seq, p);
  l2n3(msg_hdr->frag_off, p);
  l2n3(msg_hdr->frag_len, p);

  return p;
}

unsigned int dtls1_min_mtu(void) {
  return kMinMTU;
}

void dtls1_get_message_header(uint8_t *data,
                              struct hm_header_st *msg_hdr) {
  memset(msg_hdr, 0x00, sizeof(struct hm_header_st));
  msg_hdr->type = *(data++);
  n2l3(data, msg_hdr->msg_len);

  n2s(data, msg_hdr->seq);
  n2l3(data, msg_hdr->frag_off);
  n2l3(data, msg_hdr->frag_len);
}

void dtls1_get_ccs_header(uint8_t *data, struct ccs_header_st *ccs_hdr) {
  memset(ccs_hdr, 0x00, sizeof(struct ccs_header_st));

  ccs_hdr->type = *(data++);
}

int dtls1_shutdown(SSL *s) {
  int ret;
  ret = ssl3_shutdown(s);
  return ret;
}
