package main

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
)

const installerSHA = "8cc3e89859a0d669910483622741180c1f60a605bbb924f65c60df18ddd8dc1c"
const installerSize int64 = 508253841

func megaStream(key string) (cipher.Stream, error) {
	raw, err := base64.RawURLEncoding.DecodeString(key)
	if err != nil || len(raw) != 32 {
		return nil, fmt.Errorf("invalid download key")
	}
	k := make([]byte, 16)
	iv := make([]byte, 16)
	for i := range k {
		k[i] = raw[i] ^ raw[i+16]
	}
	copy(iv, raw[16:24])
	block, err := aes.NewCipher(k)
	if err != nil {
		return nil, err
	}
	return cipher.NewCTR(block, iv), nil
}

type downloadProgress struct {
	done int64
	last time.Time
}

func (p *downloadProgress) Write(b []byte) (int, error) {
	p.done += int64(len(b))
	if time.Since(p.last) > time.Second {
		progress("download", int(p.done*100/installerSize))
		p.last = time.Now()
	}
	return len(b), nil
}
func downloadInstaller(path string) error {
	if sum, n, err := fileHash(path); err == nil && sum == installerSHA && n == installerSize {
		return nil
	}
	client := &http.Client{Timeout: 2 * time.Hour}
	response, err := client.Post("https://g.api.mega.co.nz/cs?id=174318201", "application/json", bytes.NewBufferString(`[{"a":"g","g":1,"p":"Qwp2BKwQ","ssl":2}]`))
	if err != nil {
		return err
	}
	raw, err := io.ReadAll(io.LimitReader(response.Body, 65536))
	response.Body.Close()
	if err != nil {
		return err
	}
	if response.StatusCode != 200 {
		return fmt.Errorf("download service: HTTP %d", response.StatusCode)
	}
	var meta []struct {
		URL  string `json:"g"`
		Size int64  `json:"s"`
	}
	if err = json.Unmarshal(raw, &meta); err != nil || len(meta) != 1 || meta[0].Size != installerSize {
		return fmt.Errorf("UAP download unavailable: %s", raw)
	}
	meta[0].URL = strings.Replace(meta[0].URL, "http://", "https://", 1)
	if len(meta[0].URL) < 8 || meta[0].URL[:8] != "https://" {
		return fmt.Errorf("insecure UAP download URL")
	}
	response, err = client.Get(meta[0].URL)
	if err != nil {
		return err
	}
	defer response.Body.Close()
	if response.StatusCode != 200 {
		return fmt.Errorf("UAP download: HTTP %d", response.StatusCode)
	}
	stream, err := megaStream("nkZe6neiNS2n4J3A01_r1XOhwOa5cTkGH5IzJQBI3IU")
	if err != nil {
		return err
	}
	f, err := os.OpenFile(path+".partial", os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0600)
	if err != nil {
		return err
	}
	n, err := io.Copy(io.MultiWriter(f, &downloadProgress{}), &cipher.StreamReader{S: stream, R: io.LimitReader(response.Body, installerSize+1)})
	if e := f.Sync(); err == nil {
		err = e
	}
	if e := f.Close(); err == nil {
		err = e
	}
	if err != nil {
		return err
	}
	sum, _, err := fileHash(path + ".partial")
	if err != nil {
		return err
	}
	if n != installerSize || sum != installerSHA {
		return fmt.Errorf("UAP download checksum mismatch")
	}
	return os.Rename(path+".partial", path)
}
