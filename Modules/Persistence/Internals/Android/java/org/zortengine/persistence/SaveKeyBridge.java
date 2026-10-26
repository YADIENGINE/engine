package org.zortengine.persistence;

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import java.security.InvalidKeyException;
import java.security.KeyStore;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** Wraps the common native data key; the Keystore wrapping key is never exported. */
public final class SaveKeyBridge {
    private static final int NONCE_BYTES = 12;
    private static final int TAG_BITS = 128;
    private SaveKeyBridge() {}

    public static synchronized byte[] process(String alias, byte[] bytes, boolean encrypt) throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        if (!store.containsAlias(alias)) {
            if (!encrypt) throw new InvalidKeyException("Save wrapping key is missing");
            KeyGenerator generator = KeyGenerator.getInstance("AES", "AndroidKeyStore");
            generator.init(new KeyGenParameterSpec.Builder(alias,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setKeySize(256).setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setUserAuthenticationRequired(false).build());
            generator.generateKey();
        }
        SecretKey key = (SecretKey) store.getKey(alias, null);
        if (key == null) throw new InvalidKeyException("Save wrapping key is unavailable");
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        if (encrypt) {
            try {
                cipher.init(Cipher.ENCRYPT_MODE, key);
                byte[] encrypted = cipher.doFinal(bytes);
                byte[] iv = cipher.getIV();
                if (iv.length != NONCE_BYTES) throw new InvalidKeyException("Unexpected wrapping nonce");
                byte[] result = Arrays.copyOf(iv, iv.length + encrypted.length);
                System.arraycopy(encrypted, 0, result, iv.length, encrypted.length);
                return result;
            } finally { Arrays.fill(bytes, (byte) 0); }
        }
        if (bytes.length != NONCE_BYTES + 32 + TAG_BITS / 8)
            throw new InvalidKeyException("Invalid wrapped save key length");
        cipher.init(Cipher.DECRYPT_MODE, key, new GCMParameterSpec(TAG_BITS, bytes, 0, NONCE_BYTES));
        return cipher.doFinal(bytes, NONCE_BYTES, bytes.length - NONCE_BYTES);
    }

    public static synchronized void reset(String alias) throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        store.deleteEntry(alias);
    }
}
