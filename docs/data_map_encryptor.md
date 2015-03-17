# Data Map Encryption #
Encryption of the data map uses 256-bit AES in CBC mode. The key/iv are taken from the output of SHA512, which is given a secret 512-bit key from the parent container, a secret 512-bit key from the current container, a publicly known 64-bit counter, and a publicly known randomly generated 64-bit value. The ciphertext, counter, and 64-bit random value are authenticated using HMAC-SHA512, with two entirely different secret 512-bit keys.

## Diagram ##
```
                      *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
     Parent Container :  512-HMAC-Key  :  512-Cipher-Key  :
                      *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
                            |                  |
+---------------------------+                  |
|       +--------------------------------------+
|       |             *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
|       |   Container :  512-HMAC-Key  :  512-Cipher-Key  :  64-bit Version Number  :  Contents  :
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
### Input Selection ###
