#ifndef CRYPTO_HPP
#define	CRYPTO_HPP

#include <string>
#include <cmath>

#include "deps/crypto/sha1.h"
#include "deps/crypto/md5.h"

namespace SimpleWeb {
    //type must support size(), resize() and operator[]
    namespace Crypto {
        namespace Base64 {
			static const std::string base64_chars =
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"abcdefghijklmnopqrstuvwxyz"
				"0123456789+/";


			static inline bool is_base64(unsigned char c) {
				return (isalnum(c) || (c == '+') || (c == '/'));
			}

            template<class type>
            void encode(const type& ascii, type& base64) {
				unsigned char const* bytes_to_encode = (unsigned char const*)&ascii[0];
				type& ret = base64;
				int in_len = ascii.size();

				int i = 0;
				int j = 0;
				unsigned char char_array_3[3];
				unsigned char char_array_4[4];

				while (in_len--) {
					char_array_3[i++] = *(bytes_to_encode++);
					if (i == 3) {
						char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
						char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
						char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
						char_array_4[3] = char_array_3[2] & 0x3f;

						for (i = 0; (i <4); i++)
							ret += base64_chars[char_array_4[i]];
						i = 0;
					}
				}

				if (i)
				{
					for (j = i; j < 3; j++)
						char_array_3[j] = '\0';

					char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
					char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
					char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
					char_array_4[3] = char_array_3[2] & 0x3f;

					for (j = 0; (j < i + 1); j++)
						ret += base64_chars[char_array_4[j]];

					while ((i++ < 3))
						ret += '=';

				}

				/*
                BIO *bio, *b64;
                BUF_MEM *bptr;

                b64 = BIO_new(BIO_f_base64());
                BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
                bio = BIO_new(BIO_s_mem());
                BIO_push(b64, bio);
                BIO_get_mem_ptr(b64, &bptr);

                //Write directly to base64-buffer to avoid copy
                int base64_length=round(4*ceil((double)ascii.size()/3.0));
                base64.resize(base64_length);
                bptr->length=0;
                bptr->max=base64_length+1;
                bptr->data=(char*)&base64[0];

                BIO_write(b64, &ascii[0], ascii.size());
                BIO_flush(b64);

                //To keep &base64[0] through BIO_free_all(b64)
                bptr->length=0;
                bptr->max=0;
                bptr->data=nullptr;

                BIO_free_all(b64);
				*/
            }
            template<class type>
            type encode(const type& ascii) {
                type base64;
                encode(ascii, base64);
                return base64;
            }
            
            template<class type>
            void decode(const type& base64, type& ascii) {
				const type& encoded_string = base64;
				type& ret = ascii;

				int in_len = encoded_string.size();
				int i = 0;
				int j = 0;
				int in_ = 0;
				unsigned char char_array_4[4], char_array_3[3];

				while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
					char_array_4[i++] = encoded_string[in_]; in_++;
					if (i == 4) {
						for (i = 0; i <4; i++)
							char_array_4[i] = base64_chars.find(char_array_4[i]);

						char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
						char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
						char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

						for (i = 0; (i < 3); i++)
							ret += char_array_3[i];
						i = 0;
					}
				}

				if (i) {
					for (j = i; j <4; j++)
						char_array_4[j] = 0;

					for (j = 0; j <4; j++)
						char_array_4[j] = base64_chars.find(char_array_4[j]);

					char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
					char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
					char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

					for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
				}
            }
            template<class type>
            type decode(const type& base64) {
                type ascii;
                decode(base64, ascii);
                return ascii;
            }

        }
        
        template<class type>
        void MD5(const type& input, type& hash) {
			MD5Context context;
            MD5Init(&context);
            MD5Update(&context, &input[0], input.size());
            
            hash.resize(128/8);
            MD5Final((unsigned char*)&hash[0], &context);
        }
        template<class type>
        type MD5(const type& input) {
            type hash;
            MD5(input, hash);
            return hash;
        }

        template<class type>
        void SHA1(const type& input, type& hash) {
            sha1_ctx context;
            sha1_init(&context);
			sha1_update(&context, input.size(), (UINT8*)&input[0]);
            
			hash.resize(SHA1_DIGEST_SIZE);
			sha1_final(&context);
			sha1_digest(&context, SHA1_DIGEST_SIZE, (unsigned char*)&hash[0]);
        }
        template<class type>
        type SHA1(const type& input) {
            type hash;
            SHA1(input, hash);
            return hash;
        }
    }
}
#endif	/* CRYPTO_HPP */

