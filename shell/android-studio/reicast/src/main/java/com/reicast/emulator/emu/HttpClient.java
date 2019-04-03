package com.reicast.emulator.emu;

import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

public class HttpClient {
    // Called from native code
    public int openUrl(String url_string, byte[][] content, String[] contentType)
    {
        try {
            URL url = new URL(url_string);
            HttpURLConnection conn = (HttpURLConnection)url.openConnection();
            conn.connect();
            if (conn.getResponseCode() < 200 || conn.getResponseCode() >= 300)
                throw new IOException(conn.getResponseCode() + " " + conn.getResponseMessage());

            InputStream is = conn.getInputStream();

            byte[] buffer = new byte[1024];
            int length;

            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            while ((length = is.read(buffer)) > 0) {
                baos.write(buffer, 0, length);
            }
            is.close();
            baos.close();
            content[0] = baos.toByteArray();
            if (contentType != null)
                contentType[0] = conn.getContentType();

            return conn.getResponseCode();
        } catch (MalformedURLException mue) {
            Log.e("reicast", "Malformed URL", mue);
            return 500;
        } catch (IOException ioe) {
            Log.e("reicast", "I/O error", ioe);
            return 500;
        } catch (SecurityException se) {
            Log.e("reicast", "Security error", se);
            return 500;
        }
    }

    public native void nativeInit();
}
