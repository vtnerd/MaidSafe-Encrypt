# Data Map Encryption #
Encryption of the data map uses 256-bit AES in CBC mode. The key/iv are taken from the output of SHA512, which is given a secret 512-bit key from the parent, a secret 512-bit key from the current, a publicly known 64-bit counter, and a publicly known randomly generated 64-bit value. The ciphertext, counter, and 64-bit random value are authenticated using HMAC-SHA512, with two entirely different secret 512-bit keys.

## Diagram ##
```
                      *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
               Parent :  512-HMAC-Key  :  512-Cipher-Key  :
                      *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
                            |                  |
+---------------------------+                  |
|       +--------------------------------------+
|       |             *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
|       |     Current :  512-HMAC-Key  :  512-Cipher-Key  :  64-bit Version Number  :  Contents  :
|       |             *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
|       |                         |            |                          |                |
|  +--  |  -----------------------+            |         +------- ++1 ----+                |
|  |    |                                      |         |                                 |
|  |    |            *~~~~~~~~~~~~~~*          |         |                                 |
|  |    |      CSRNG :  64-bit Out  :          |         |                                 |
|  |    |            *~~~~~~~~~~~~~~*          |         +---------------------+           |
|  |    |                   |                  |         |                     |           |
|  |    +-----------------  |  -----------+    |         |                     |           |
|  |                   +----+--------+    |    |         |                     |           |
|  |                   |             |    |    |         |                     |           |
|  |                   |             \/   \/   \/        \/                    |           |
|  |                   |           *~~~~~~~~~~~~~~~~~~~~~~~~~*                 |           |
|  |                   |    SHA512 :          Input          :                 |           |
|  |                   |           *~~~~~~~~~~~~~~~~~~~~~~~~~*                 |           |
|  |                   |           :  256-bits  :  256 bits  :                 |           |
|  |                   |           *~~~~~~~~~~~~~~~~~~~~~~~~~*                 |           |
|  |                   |                   \         /           +-----------  |  ---------+
|  |                   |                    |       |            |             | 
|  |                   |                    \/     \/            \/            |
|  |                   |                *~~~~~~~~~~~~~~~~~~~~~~~~~~~~*         |
|  |                   |   AES-256-CBC: :  KEY  :  IV  :  Plaintext  :         |
|  |                   |                *~~~~~~~~~~~~~~~~~~~~~~~~~~~~*         |
|  |                   |                       :  Ciphertext  :                |
|  |                   +--------------+        *~~~~~~~~~~~~~~*                |
|  |                                  |                |                       |
|  |                                  \/               \/                      |
|  |                         *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*   |
 \  \ Data Stored on Network :  64-bit Random  :  Ciphertext  :  HMAC-512  :   |
  \  \                       *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*   |
   \  \                                       \  |                 /\          |
    \  ----------------------------------+     | |                 |           |
      --------------------------------+  |     | |   +-----------  |  ---------+
                                      |  |     | |   |             |
                                     \/  \/   \/ \/ \/             |
                                    *~~~~~~~~~~~~~~~~~*            |
                           HMAC-512 :  Key  :  Input  :            |
                                    *~~~~~~~~~~~~~~~~~*            |
                                         :  HMAC  :                |
                                         *~~~~~~~~*                |
                                             |                     |
                                             +---------------------+
```

## AES-256-CBC ##
### Cipher Selection ###
AES was chosen due to its ubiqituous use - it is [widely studied](http://blog.cryptographyengineering.com/2012/10/so-you-want-to-use-alternative-cipher.html).

### Block Mode Selection ###
CBC was chosen as the block mode because of its resilience against key/iv re-use when data is at rest. The plaintext is always put through the cipher, so catastrophic `Plaintext1 ^ Plaintext2` never appear like in other modes (CTR, CFB for example). The only information leaked with key/iv re-use in CBC mode are the number of prefix blocks that remained unchanged. This could let an attacker know that certain metadata has remained unchanged, so a 64-bit counter was used in the [KDF](#sha512-kdf) to guarantee that the key/iv is changed each time a container version is stored.

However, CBC mode can be used incorrectly if an attacker knows the ciphertext of block `N`, and can manipulate the plaintext for block `N+1` (chosen plaintext attack). The data map encryptor encrypts the _entire_ contents at once, so the only ciphertext block an attacker can know in advance is the IV. The [KDF](#sha512-kdf) incorporates a 64-bit random value to make the IV unpredictable in case the 64-bit counter wraps. So a chosen plaintext attack against CBC would require a poor RNG, and 2^64 versions (counter) of the container.

## SHA512 KDF ##
### Hash Function Selection ###
SHA512 is used throughout SAFE, so its use was continued here. HKDF was considered since the intended use is _exactly_ for this situation, however Cryptopp does not have an implementation. Rather than implement on our own, we went with SHA512 and kept the input fixed in length (no length extension attacks).

### Input Selection ###
The KDF is `SHA512(Parent 512-bit Cipher Key || 512-bit Cipher Key || 64-bit counter || 64-bit random value)` where `||` denotes concatenation.

##### Parent 512-bit Cipher Key #####
Previously the `[finish thought]`.

##### 512-bit Cipher Key #####
> * This value is kept secret.
> * This value is different from the 512-bit HMAC Key

Guarantees that the Cipher key is different from its parent.

##### 64-bit Counter #####
Each SDV stored on the network has an incremented counter used for identification. This counter is provided to the encryption routine, which guarantees that the key/iv changes between versions, even if the random number generator is poor. This makes chosen plaintext attacks more difficult (have to wait 2^64 cycles), and ensures information leakage between versions is not possible.

##### 64-bit Random Value #####
Chosen plaintext attacks are theoretically possible if the 64-bit counter cycles. Including a random value makes the attack more difficult since the key/iv will be unpredictable even after the counter cycles.

## HMAC-SHA512 ##
### Authentication Selection ###
### Key Selection ###
The key to the HMAC is `Parent 512-bit HMAC Key || 512-bit HMAC Key` where `||` denotes concatenation.
##### Parent 512-bit HMAC Key #####
> * This value is kept secret.
> * This value is different from the Parent 512-bit Cipher Key

The network-address for the [SDV](https://github.com/maidsafe/MaidSafe-Common/blob/next/docs/structured_data_versions_update.md) used by a [Container](https://github.com/maidsafe/MaidSafe-Common/blob/next/docs/posix_api.md) is the SHA512 of its HMAC-key. An attacker could brute force this value offline, or could accidentally generate the same value if RNG is poor. Mixing the parent HMAC key makes this difficult since the parent container is never referenced by the container. So a brute forcing `Container 512-bit HMAC key -> NAE (SHA512)` would only yield half of the value needed to generated a HMAC.

##### 512-bit HMAC Key #####
> * This value is kept secret.
> * This value is different from the 512-bit Cipher Key

Guarantees that the HMAC key is different from its parent.

### Input Selection ###
##### Ciphertext #####
##### 64-bit Counter #####
##### 64-bit Random Value #####
