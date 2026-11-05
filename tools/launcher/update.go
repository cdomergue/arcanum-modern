package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"strconv"
	"strings"
	"time"
)

const releaseAPI = "https://api.github.com/repos/cdomergue/arcanum-modern/releases/latest"

var zipName = archiveName()

const manifestName = "ArcanumModern/manifest.json"
const maxArchive = int64(2 << 30)

var buildVersion = "0.0.0"

type asset struct {
	Name string `json:"name"`
	URL  string `json:"browser_download_url"`
	Size int64  `json:"size"`
}
type release struct {
	Tag        string  `json:"tag_name"`
	Body       string  `json:"body"`
	Published  string  `json:"published_at"`
	Draft      bool    `json:"draft"`
	Prerelease bool    `json:"prerelease"`
	Assets     []asset `json:"assets"`
}

func version(tag string) ([3]uint64, bool) {
	var out [3]uint64
	if len(tag) > 64 {
		return out, false
	}
	parts := strings.Split(strings.TrimPrefix(tag, "v"), ".")
	if len(parts) != 3 {
		return out, false
	}
	for i, p := range parts {
		if p == "" || (len(p) > 1 && p[0] == '0') {
			return out, false
		}
		for _, c := range p {
			if c < '0' || c > '9' {
				return out, false
			}
		}
		n, e := strconv.ParseUint(p, 10, 64)
		if e != nil {
			return out, false
		}
		out[i] = n
	}
	return out, true
}
func newer(current, tag string) bool {
	a, ok := version(current)
	b, valid := version(tag)
	if !ok || !valid {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return b[i] > a[i]
		}
	}
	return false
}
func updateClient() *http.Client {
	return &http.Client{Timeout: 15 * time.Minute, CheckRedirect: func(r *http.Request, via []*http.Request) error {
		if len(via) > 5 || r.URL.Scheme != "https" {
			return fmt.Errorf("unsafe redirect")
		}
		switch r.URL.Host {
		case "github.com", "release-assets.githubusercontent.com", "objects.githubusercontent.com":
			return nil
		}
		return fmt.Errorf("unexpected download host")
	}}
}
func request(ctx context.Context, c *http.Client, address string) (*http.Response, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, address, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", "Arcanum-Renaissance-Launcher/"+buildVersion)
	req.Header.Set("Accept", "application/vnd.github+json")
	res, err := c.Do(req)
	if err != nil {
		return nil, err
	}
	if res.StatusCode != http.StatusOK {
		res.Body.Close()
		return nil, fmt.Errorf("HTTP %d", res.StatusCode)
	}
	return res, nil
}
func readRemote(ctx context.Context, c *http.Client, address string, limit int64) ([]byte, error) {
	res, err := request(ctx, c, address)
	if err != nil {
		return nil, err
	}
	defer res.Body.Close()
	b, err := io.ReadAll(io.LimitReader(res.Body, limit+1))
	if err == nil && int64(len(b)) > limit {
		err = fmt.Errorf("response too large")
	}
	return b, err
}
func latest(ctx context.Context, c *http.Client, endpoint string) (release, error) {
	ctx, cancel := context.WithTimeout(ctx, 6*time.Second)
	defer cancel()
	var r release
	b, err := readRemote(ctx, c, endpoint, 4<<20)
	if err != nil {
		return r, err
	}
	if err = json.Unmarshal(b, &r); err != nil {
		return r, err
	}
	_, valid := version(r.Tag)
	if !valid || r.Draft || r.Prerelease || r.Published == "" {
		return r, fmt.Errorf("not a published stable release")
	}
	return r, nil
}
func (r release) downloadAsset(name string) (asset, error) {
	var found asset
	for _, a := range r.Assets {
		if a.Name == name {
			if found.Name != "" {
				return asset{}, fmt.Errorf("duplicate asset")
			}
			found = a
		}
	}
	expected := "https://github.com/cdomergue/arcanum-modern/releases/download/" + r.Tag + "/" + name
	u, err := url.Parse(found.URL)
	if err != nil || found.URL != expected || u.User != nil || found.Size <= 0 || found.Size > maxArchive {
		return asset{}, fmt.Errorf("missing or invalid release asset: %s", name)
	}
	return found, nil
}
func archiveChecksum(data []byte) (string, error) {
	digest := ""
	for _, line := range strings.Split(string(data), "\n") {
		f := strings.Fields(line)
		if len(f) != 2 || strings.TrimPrefix(f[1], "*") != zipName {
			continue
		}
		b, e := hex.DecodeString(f[0])
		if e != nil || len(b) != 32 || digest != "" {
			return "", fmt.Errorf("invalid archive checksum")
		}
		digest = strings.ToLower(f[0])
	}
	if digest == "" {
		return "", fmt.Errorf("archive checksum missing")
	}
	return digest, nil
}

type progressWriter struct {
	done, total int64
	report      func(int)
}

func (p *progressWriter) Write(b []byte) (int, error) {
	p.done += int64(len(b))
	p.report(int(p.done * 100 / p.total))
	return len(b), nil
}
func download(ctx context.Context, c *http.Client, r release, destination string, report func(int)) error {
	a, err := r.downloadAsset(zipName)
	if err != nil {
		return err
	}
	sums, err := r.downloadAsset("SHA256SUMS.txt")
	if err != nil {
		return err
	}
	data, err := readRemote(ctx, c, sums.URL, 64<<10)
	if err != nil {
		return err
	}
	expected, err := archiveChecksum(data)
	if err != nil {
		return err
	}
	res, err := request(ctx, c, a.URL)
	if err != nil {
		return err
	}
	defer res.Body.Close()
	f, err := os.OpenFile(destination, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		return err
	}
	h := sha256.New()
	p := &progressWriter{total: a.Size, report: report}
	n, err := io.Copy(io.MultiWriter(f, h, p), io.LimitReader(res.Body, a.Size+1))
	closeErr := f.Close()
	if err != nil {
		return err
	}
	if closeErr != nil {
		return closeErr
	}
	if n != a.Size || hex.EncodeToString(h.Sum(nil)) != expected {
		return fmt.Errorf("download checksum mismatch")
	}
	return nil
}
