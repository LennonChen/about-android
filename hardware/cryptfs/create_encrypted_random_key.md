create_encrypted_random_key
========================================

path: system/vold/cryptfs.c
```
static int create_encrypted_random_key(
       char *passwd,
       unsigned char *master_key,
       unsigned char *salt,
       struct crypt_mnt_ftr *crypt_ftr)
{
    int fd;
    unsigned char key_buf[KEY_LEN_BYTES];
    EVP_CIPHER_CTX e_ctx;
    int encrypted_len, final_len;

    /* Get some random bits for a key */
    fd = open("/dev/urandom", O_RDONLY);
    // 产生随机16 Bytes DEK(disk encryption key--磁盘加密用的密钥)及16 Bytes SALT；
    read(fd, key_buf, sizeof(key_buf));
    read(fd, salt, SALT_LEN);
    close(fd);

    /* Now encrypt it with the password */
    return encrypt_master_key(passwd, salt, key_buf, master_key, crypt_ftr);
}
```

encrypt_master_key
----------------------------------------

path: system/vold/cryptfs.c
```
static int encrypt_master_key(const char *passwd, const unsigned char *salt,
                              const unsigned char *decrypted_master_key,
                              unsigned char *encrypted_master_key,
                              struct crypt_mnt_ftr *crypt_ftr)
{
    unsigned char ikey[32+32] = { 0 }; /* Big enough to hold a 256 bit key and 256 bit IV */
    EVP_CIPHER_CTX e_ctx;
    int encrypted_len, final_len;
    int rc = 0;

    /* Turn the password into an intermediate key and IV that can decrypt the master key */
    get_device_scrypt_params(crypt_ftr);

    switch (crypt_ftr->kdf_type) {
    case KDF_SCRYPT_KEYMASTER_UNPADDED:
    case KDF_SCRYPT_KEYMASTER_BADLY_PADDED:
    case KDF_SCRYPT_KEYMASTER:
    // 1.将IK1填充到硬件产生的私钥规格大小(目前看到是RSA算法，256Bytes), 具体是:
    // 00 || IK1 || 00..00 ## one zero byte, 32 IK1 bytes, 223 zero bytes.
    //
    // 2.使用硬件私钥 HBK 对 IK1 进行签名，生成256 Bytes签名数据作为IK2；
    // 3.对(IK2+SALT)使用scrypt算法(与第二步中的SALT相同)产生出32 Bytes HASH 作为IK3；
    // 4.使用IK3前16 Bytes作为KEK(用来加密主密钥DEK的KEY)，后16 Bytes作为算法IV(初始化向量)；
        if (keymaster_create_key(crypt_ftr)) {
            SLOGE("keymaster_create_key failed");
            return -1;
        }

        if (scrypt_keymaster(passwd, salt, ikey, crypt_ftr)) {
            SLOGE("scrypt failed");
            return -1;
        }
        break;

    case KDF_SCRYPT:
        // 对(用户密码+SALT)使用scrypt算法产生32 Bytes HASH 作为IK1(intermediate key 1);
        if (scrypt(passwd, salt, ikey, crypt_ftr)) {
            SLOGE("scrypt failed");
            return -1;
        }
        break;

    default:
        SLOGE("Invalid kdf_type");
        return -1;
    }

    /* Initialize the decryption engine */
    if (! EVP_EncryptInit(&e_ctx, EVP_aes_128_cbc(), ikey, ikey+KEY_LEN_BYTES)) {
        SLOGE("EVP_EncryptInit failed\n");
        return -1;
    }
    EVP_CIPHER_CTX_set_padding(&e_ctx, 0); /* Turn off padding as our data is block aligned */

    /* Encrypt the master key */
    /* 加密master key */
    if (! EVP_EncryptUpdate(&e_ctx, encrypted_master_key, &encrypted_len,
                              decrypted_master_key, KEY_LEN_BYTES)) {
        SLOGE("EVP_EncryptUpdate failed\n");
        return -1;
    }
    if (! EVP_EncryptFinal(&e_ctx, encrypted_master_key + encrypted_len, &final_len)) {
        SLOGE("EVP_EncryptFinal failed\n");
        return -1;
    }

    if (encrypted_len + final_len != KEY_LEN_BYTES) {
        SLOGE("EVP_Encryption length check failed with %d, %d bytes\n", encrypted_len, final_len);
        return -1;
    }

    /* Store the scrypt of the intermediate key, so we can validate if it's a
       password error or mount error when things go wrong.
       Note there's no need to check for errors, since if this is incorrect, we
       simply won't wipe userdata, which is the correct default behavior
    */
    int N = 1 << crypt_ftr->N_factor;
    int r = 1 << crypt_ftr->r_factor;
    int p = 1 << crypt_ftr->p_factor;

    // 使用AES_CBC算法，采用KEK作为密钥，IV作为初始化向量来加密用户的主密钥DEK，
    // 生成加密后的主密钥， 存入分区尾部数据结构中；
    rc = crypto_scrypt(ikey, KEY_LEN_BYTES,
                       crypt_ftr->salt, sizeof(crypt_ftr->salt), N, r, p,
                       crypt_ftr->scrypted_intermediate_key,
                       sizeof(crypt_ftr->scrypted_intermediate_key));

    if (rc) {
      SLOGE("encrypt_master_key: crypto_scrypt failed");
    }

    return 0;
}
```