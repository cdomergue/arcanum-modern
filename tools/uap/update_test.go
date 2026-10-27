package main

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

func TestNewerRelease(t *testing.T) {
	for _, tc := range []struct {
		current, latest string
		want            bool
	}{
		{"1.4.0", "v1.4.1", true}, {"1.4.0", "v1.10.0", true},
		{"1.10.0", "v1.9.99", false}, {"1.4.0", "v2.0.0", true},
		{"1.4.0", "v1.4.0", false}, {"1.4.0", "1.4.1", true},
		{"1.4.0", "v1.5.0-rc1", false}, {"1.4.0", "v01.5.0", false},
		{"1.4.0", "v1.5", false}, {"1.4.0", "v1.5.0/evil", false},
		{"1.4.0", "v1.18446744073709551616.0", false},
		{"garbage", "v2.0.0", false}, {"1.4.0", "v1.+5.0", false},
	} {
		if got := newerRelease(tc.current, tc.latest); got != tc.want {
			t.Errorf("%s -> %s: %v", tc.current, tc.latest, got)
		}
	}
}

func TestLatestUpdate(t *testing.T) {
	valid := `{"tag_name":"v1.5.0","draft":false,"prerelease":false,"published_at":"2026-10-28T00:00:00Z"}`
	for _, tc := range []struct {
		name, body, want string
		status           int
	}{
		{"new", valid, "v1.5.0", 200},
		{"equal", strings.Replace(valid, "1.5.0", "1.4.0", 1), "", 200},
		{"older", strings.Replace(valid, "1.5.0", "1.3.0", 1), "", 200},
		{"draft", strings.Replace(valid, `"draft":false`, `"draft":true`, 1), "", 200},
		{"preview", strings.Replace(valid, `"prerelease":false`, `"prerelease":true`, 1), "", 200},
		{"unpublished", `{"tag_name":"v1.5.0"}`, "", 200},
		{"rate limit", valid, "", 403}, {"unavailable", valid, "", 503},
		{"missing", valid, "", 404}, {"bad json", "<html>", "", 200},
		{"oversized", valid + strings.Repeat(" ", 1<<20), "", 200},
	} {
		t.Run(tc.name, func(t *testing.T) {
			server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				if r.Method != "GET" || r.Header.Get("Accept") != "application/vnd.github+json" || r.Header.Get("User-Agent") != "Arcanum-Renaissance/1.4.0" {
					t.Error("invalid request")
				}
				if r.Header.Get("Authorization") != "" {
					t.Error("unexpected credentials")
				}
				w.WriteHeader(tc.status)
				_, _ = w.Write([]byte(tc.body))
			}))
			defer server.Close()
			if got := latestUpdate(server.Client(), server.URL, "1.4.0"); got != tc.want {
				t.Fatalf("got %q, want %q", got, tc.want)
			}
		})
	}
}

func TestUpdateTimeoutAndOffline(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { <-r.Context().Done() }))
	client := server.Client()
	client.Timeout = 30 * time.Millisecond
	start := time.Now()
	if latestUpdate(client, server.URL, "1.4.0") != "" {
		t.Fatal("timeout offered update")
	}
	if time.Since(start) > time.Second {
		t.Fatal("timeout did not bound request")
	}
	server.Close()
	if latestUpdate(client, server.URL, "1.4.0") != "" {
		t.Fatal("offline offered update")
	}
}
