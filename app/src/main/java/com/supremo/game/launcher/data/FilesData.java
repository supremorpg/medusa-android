package com.supremo.launcher.launcher.data;

public class FilesData {

    private String name;
    private long size;
    private String path;
    private String url;
    private String hash;
    private String hashAlgorithm;

    public FilesData(String name, long size, String path, String url) {
        this(name, size, path, url, "", "");
    }

    public FilesData(String name, long size, String path, String url, String hash) {
        this(name, size, path, url, hash, inferHashAlgorithm(hash));
    }

    public FilesData(
            String name,
            long size,
            String path,
            String url,
            String hash,
            String hashAlgorithm
    ) {
        this.name = name;
        this.size = size;
        this.path = path;
        this.url = url;
        this.hash = hash == null ? "" : hash.trim();
        this.hashAlgorithm = hashAlgorithm == null ? "" : hashAlgorithm.trim().toLowerCase();
    }

    public String getName()
    {
        return name;
    }

    public long getSize()
    {
        return size;
    }

    public  String getPath()
    {
        return path;
    }

    public  String getUrl()
    {
        return url;
    }

    public String getHash()
    {
        return hash;
    }

    public String getHashAlgorithm() {
        return hashAlgorithm;
    }

    private static String inferHashAlgorithm(String value) {
        if (value == null) {
            return "";
        }
        String normalized = value.trim().toLowerCase();
        if (normalized.startsWith("sha256:") || normalized.length() == 64) {
            return "sha256";
        }
        if (normalized.startsWith("sha1:") || normalized.length() == 40) {
            return "sha1";
        }
        if (normalized.startsWith("md5:") || normalized.length() == 32) {
            return "md5";
        }
        return "";
    }

}
