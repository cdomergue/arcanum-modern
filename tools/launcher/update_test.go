package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestVersionAndRelease(t *testing.T) {
	for _, s := range []string{"", "v1.2", "v01.2.3", "1.2.3-beta", "../../bad", "1.2.-1", "1.2.18446744073709551616"} {
		if _, ok := version(s); ok {
			t.Error(s)
		}
	}
	for _, s := range []string{"2.0.2", "v2.1.0", "3.0.0"} {
		if !newer("v2.0.1", s) {
			t.Error(s)
		}
	}
	for _, s := range []string{"2.0.1", "v1.9.9", "2.0.0"} {
		if newer("2.0.1", s) {
			t.Error(s)
		}
	}
	r := release{Tag: "v2.0.2", Published: "2026-11-05T12:00:00Z", Body: "## New\n\n- A feature"}
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, req *http.Request) { _ = json.NewEncoder(w).Encode(r) }))
	defer server.Close()
	if got, e := latest(context.Background(), server.Client(), server.URL); e != nil || got.Body != r.Body {
		t.Fatalf("release: %+v %v", got, e)
	}
	r.Draft = true
	if _, e := latest(context.Background(), server.Client(), server.URL); e == nil {
		t.Fatal("draft accepted")
	}
	r.Draft = false
	r.Prerelease = true
	if _, e := latest(context.Background(), server.Client(), server.URL); e == nil {
		t.Fatal("prerelease accepted")
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, e := latest(ctx, server.Client(), server.URL); e == nil {
		t.Fatal("cancel ignored")
	}
}

type transportFunc func(*http.Request) (*http.Response, error)

func (f transportFunc) RoundTrip(r *http.Request) (*http.Response, error) { return f(r) }
func TestVerifiedDownloadAndInstall(t *testing.T) {
	dir := t.TempDir()
	zip := zipFixture(t, dir, nil, nil)
	bytes, _ := os.ReadFile(zip)
	hash := sha256.Sum256(bytes)
	sum := hex.EncodeToString(hash[:])
	bad := false
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if strings.HasSuffix(r.URL.Path, "SHA256SUMS.txt") {
			fmt.Fprintf(w, "%s  %s\n", sum, zipName)
		} else {
			if bad {
				_, _ = w.Write([]byte("corrupt"))
			} else {
				_, _ = w.Write(bytes)
			}
		}
	}))
	defer server.Close()
	// HTTPS URL validation stays enabled; only the test transport maps trusted
	// release URLs to the local fixture server. No endpoint override ships.
	client := server.Client()
	transport := client.Transport
	client.Transport = transportFunc(func(r *http.Request) (*http.Response, error) {
		r.URL.Scheme = "http"
		r.URL.Host = strings.TrimPrefix(server.URL, "http://")
		return transport.RoundTrip(r)
	})
	r := release{Tag: "v2.0.2"}
	for _, name := range []string{zipName, "SHA256SUMS.txt"} {
		r.Assets = append(r.Assets, asset{Name: name, URL: "https://github.com/cdomergue/arcanum-modern/releases/download/v2.0.2/" + name, Size: int64(len(bytes))})
	}
	output := filepath.Join(dir, "download.zip")
	percent := 0
	if e := download(context.Background(), client, r, output, func(p int) { percent = p }); e != nil {
		t.Fatal(e)
	}
	if percent != 100 {
		t.Fatal(percent)
	}
	payload := filepath.Join(dir, "payload")
	m, e := unpack(output, payload, r.Tag)
	if e != nil {
		t.Fatal(e)
	}
	game := filepath.Join(dir, "game é")
	fixture(t, game, "2.0.1")
	if e = install(game, payload, m, nil); e != nil {
		t.Fatal(e)
	}
	bad = true
	if e = download(context.Background(), client, r, filepath.Join(dir, "bad.zip"), func(int) {}); e == nil {
		t.Fatal("bad download accepted")
	}
	r.Assets[0].URL = "https://example.com/evil.exe"
	if _, e = r.downloadAsset(zipName); e == nil {
		t.Fatal("external executable accepted")
	}
}
func TestChecksumAndNotes(t *testing.T) {
	for _, s := range []string{"", strings.Repeat("0", 64) + "  other.zip", strings.Repeat("0", 64) + "  " + zipName + "\n" + strings.Repeat("1", 64) + "  " + zipName} {
		if _, e := archiveChecksum([]byte(s)); e == nil {
			t.Fatal("checksum accepted", s)
		}
	}
	if got := releaseText("# Changes\n\n- **Portraits** and [details](https://example.com)"); got != "Changes\r\n\r\n• Portraits and details (https://example.com)" {
		t.Fatal(got)
	}
}
func TestServiceErrorIsBounded(t *testing.T) {
	s := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { http.Error(w, "rate limited", 429) }))
	defer s.Close()
	start := time.Now()
	if _, e := latest(context.Background(), s.Client(), s.URL); e == nil {
		t.Fatal("accepted HTTP error")
	}
	if time.Since(start) > time.Second {
		t.Fatal("slow fallback")
	}
}
